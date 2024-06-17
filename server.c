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
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
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
    printf("(sock=%d) Fermeture de la socket...\n", socket);
    if (close(socket) == -1) {
        perror("Erreur lors de la fermeture de la socket");
        exit(EXIT_FAILURE);
    } else {
        printf("(sock=%d) Socket fermée !\n", socket);
    }
}

// Méthode permettant de créer une socket serveur et d'écouter les connexions entrantes
int socket_serveur(int port) {
    int server_socket;
    struct sockaddr_in server_addr;

    // Créer une socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Échec de la création de la socket");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Lier la socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Échec de la liaison");
        fermer_socket(server_socket);
        exit(EXIT_FAILURE);
    }

    // Écouter les connexions
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Échec de l'écoute");
        fermer_socket(server_socket);
        exit(EXIT_FAILURE);
    } else {
        printf("Serveur à l'écoute sur le port %d\n", port);
    }

    return server_socket;
}

// Méthode permettant d'envoyer une réponse au client
void envoyer_reponse(int socket, const char *reponse) {
    printf("(sock=%d) Envoi de la réponse: \"%s\"...\n", socket, reponse);
    if (send(socket, reponse, strlen(reponse), 0) < 0) {
        perror("Échec de l'envoi");
        fermer_socket(socket);
        exit(EXIT_FAILURE);
    } else {
        printf("(sock=%d) Réponse envoyée !\n", socket);
    }
}

// Méthode permettant de recevoir une commande du client et la retourner
char *recevoir_commande(int socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    printf("(sock=%d) Attente de la commande du client...\n", socket);
    if ((bytes_received = recv(socket, buffer, BUFFER_SIZE, 0)) < 0) {
        perror("Échec de la réception");
        fermer_socket(socket);
        exit(EXIT_FAILURE);
    } else if (bytes_received == 0) {
        printf("(sock=%d) Le client a fermé la connexion\n", socket);
        fermer_socket(socket);
        exit(EXIT_FAILURE);
    } else {
        printf("(sock=%d) Commande reçue !\n", socket);
    }

    buffer[bytes_received] = '\0';
    printf("(sock=%d) Commande du client: \"%s\"\n", socket, buffer);

    return strdup(buffer);
}

// Méthode permettant de gérer un client
void handle_client(int client_id, int client_sock) {
    int client_pid = getpid();

    for (;;) {
        char *commande = recevoir_commande(client_sock);

        int requested_amount;
        if (sscanf(commande, "REQUEST %d", &requested_amount) == 1) {
            struct sembuf sb = {0, -1, 0}; // Verrouiller
            semop(sem_id, &sb, 1);

            if (requested_amount <= resource_amount) {
                resource_amount -= requested_amount;

                // Répondre au client OK
                char buffer[BUFFER_SIZE];
                snprintf(buffer, BUFFER_SIZE, "GRANTED %d", requested_amount);
                envoyer_reponse(client_sock, buffer);
            } else {
                // Répondre au client KO
                char buffer[BUFFER_SIZE];
                snprintf(buffer, BUFFER_SIZE, "DENIED %d, REASON: Ressources insuffisantes", requested_amount);
                envoyer_reponse(client_sock, buffer);
            }

            sb.sem_op = 1; // Déverrouiller
            semop(sem_id, &sb, 1);
        } else if (sscanf(commande, "RELEASE %d", &requested_amount) == 1) {
            struct sembuf sb = {0, -1, 0}; // Verrouiller
            semop(sem_id, &sb, 1);

            resource_amount += requested_amount;

            // Répondre au client OK
            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "RELEASED %d", requested_amount);
            envoyer_reponse(client_sock, buffer);

            sb.sem_op = 1; // Déverrouiller
            semop(sem_id, &sb, 1);
        }
    }

    fermer_socket(client_sock);
    exit(0);
}

// Méthode permettant d'attendre la connexion d'un client et de la créer/gérer
int accept_client(int server_socket) {
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Attendre une connexion client
    printf("En attente d'une connexion client...\n");

    // Accepter une connexion client
    if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
        perror("Échec de l'acceptation");
        return -1;
    }

    // Afficher les informations du client
    printf("Client connecté: %s:%d sock_id=%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_socket);

    // Fork pour gérer le client
    pid_t pid = fork();
    if (pid < 0) {
        perror("Échec du fork");
        fermer_socket(client_socket);
    } else if (pid == 0) {
        // Fermer la socket serveur car le fils ne gère pas le serveur
        close(server_socket);
        // Le fils gère le client
        // TODO SEMEPHAORE ICI
        handle_client(client_count, client_socket);
    } else {
        // Fermer la socket client car le père ne gère pas le client
        close(client_socket);
        // Le père ajoute le client à la liste
        clients[client_count].client_pid = pid;
        clients[client_count].resource_amount = 0;
        strcpy(clients[client_count].client_ip, inet_ntoa(client_addr.sin_addr));
        clients[client_count].client_port = ntohs(client_addr.sin_port);
        client_count++;
        // TODO SEMEPHAORE ICI
    }

    return client_socket;
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

    // Créer une socket serveur
    server_sock = socket_serveur(port);

    // Gérer la terminaison des enfants
    signal(SIGCHLD, sigchld_handler); 

    // TODO: Gérer l'actualisation du status du serveur ici avec un fork

    for (;;) {
        // Attendre une connexion client
        accept_client(server_sock);
    }

    // Nettoyer
    fermer_socket(server_sock);
    semctl(sem_id, 0, IPC_RMID);
    return 0;
}
