#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

// Objet permettant de stocker les informations d'un client
typedef struct {
    int client_pid;
    int resource_amount;
} ClientInfo;

int resource_amount;
ClientInfo clients[MAX_CLIENTS];
int client_count = 0;
int sem_id;

// Méthode permettant d'afficher le message d'erreur d'utilisation du programme
void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <resource_amount> <port>\n", prog_name);
    exit(EXIT_FAILURE);
}

// Méthode permettant de fermer une socket
void fermer_socket(int socket) {
    printf("Fermeture de la socket %d...\n", socket);
    if (close(socket) == -1) {
        perror("Erreur lors de la fermeture de la socket %d");
        exit(EXIT_FAILURE);
    } else {
        printf("Socket %d fermée !\n", socket);
    }
}

// Méthode permettant de gérer un client
void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    int client_pid = getpid();

    while ((bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_received] = '\0';

        int requested_amount;
        if (sscanf(buffer, "REQUEST %d", &requested_amount) == 1) {
            struct sembuf sb = {0, -1, 0}; // Verrouiller
            semop(sem_id, &sb, 1);

            if (requested_amount <= resource_amount) {
                resource_amount -= requested_amount;
                snprintf(buffer, BUFFER_SIZE, "GRANTED %d", requested_amount);
            } else {
                snprintf(buffer, BUFFER_SIZE, "DENIED %d, REASON: Ressources insuffisantes", requested_amount);
            }

            sb.sem_op = 1; // Déverrouiller
            semop(sem_id, &sb, 1);

            send(client_sock, buffer, strlen(buffer), 0);
        } else if (sscanf(buffer, "RELEASE %d", &requested_amount) == 1) {
            struct sembuf sb = {0, -1, 0}; // Verrouiller
            semop(sem_id, &sb, 1);

            resource_amount += requested_amount;
            snprintf(buffer, BUFFER_SIZE, "RELEASED %d", requested_amount);

            sb.sem_op = 1; // Déverrouiller
            semop(sem_id, &sb, 1);

            send(client_sock, buffer, strlen(buffer), 0);
        }
    }

    fermer_socket(client_sock);
    exit(0);
}

// Gestionnaire de signal pour SIGCHLD
void sigchld_handler(int signum) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

// Méthode principale
int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage(argv[0]);
    }

    resource_amount = atoi(argv[1]);
    int port = atoi(argv[2]);

    // Mise en place du sémaphore
    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }

    if (semctl(sem_id, 0, SETVAL, 1) == -1) {
        perror("semctl");
        exit(EXIT_FAILURE);
    }

    int server_sock;
    struct sockaddr_in server_addr;

    // Créer une socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Échec de la création de la socket");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Lier la socket
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Échec de la liaison");
        fermer_socket(server_sock);
        exit(EXIT_FAILURE);
    }

    // Écouter les connexions
    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("Échec de l'écoute");
        fermer_socket(server_sock);
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, sigchld_handler); // Gérer la terminaison des enfants

    // TODO: Gérer l'actualisation du status du serveur ici avec un fork

    printf("Serveur à l'écoute sur le port %d\n", port);

    for (;;) {
        int client_sock;
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        // Attendre une connexion client
        printf("En attente d'une connexion client...\n");

        // Accepter une connexion client
        if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("Échec de l'acceptation");
            continue;
        }

        // Afficher les informations du client
        printf("Client connecté: %s:%d sock_id=%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_sock);

        // Fork pour gérer le client
        pid_t pid = fork();
        if (pid < 0) {
            perror("Échec du fork");
            fermer_socket(client_sock);
        } else if (pid == 0) {
            close(server_sock);
            handle_client(client_sock);
        } else {
            close(client_sock);
            clients[client_count].client_pid = pid;
            clients[client_count].resource_amount = 0;
            client_count++;
        }
    }

    // Nettoyer
    fermer_socket(server_sock);
    semctl(sem_id, 0, IPC_RMID);
    return 0;
}
