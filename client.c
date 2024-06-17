#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

int total_resources = 0;

// Méthode permettant d'afficher le message d'erreur d'utilisation du programme
void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <server_address> <server_port> <resource_amount> <delay>\n", prog_name);
    exit(EXIT_FAILURE);
}

// Méthode permettant de créer une socket client et de se connecter à un serveur
int socket_client(const char *address, int port) {
    int client_socket;
    struct sockaddr_in serveur_sockaddr;
    struct hostent *hostent;

    if ((hostent = gethostbyname(address)) == NULL) {
        perror("Erreur lors de l'appel de gethostbyname()");
        exit(EXIT_FAILURE);
    }

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Erreur lors de l'appel de socket()");
        exit(EXIT_FAILURE);
    }

    memset(&serveur_sockaddr, 0, sizeof(serveur_sockaddr));
    serveur_sockaddr.sin_family = AF_INET;
    serveur_sockaddr.sin_port = htons(port);
    memcpy(&serveur_sockaddr.sin_addr, hostent->h_addr_list[0], hostent->h_length);

    printf("Connexion à %s (%s) sur le port %d...\n", hostent->h_name, address, port);
    if (connect(client_socket, (struct sockaddr*)&serveur_sockaddr, sizeof(serveur_sockaddr)) == -1) {
        perror("Erreur lors de l'appel de connect()");
        exit(EXIT_FAILURE);
    } else {
        printf("Connecté !\n");
    }
    return client_socket;
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

// Méthode permettant d'envoyer une commande au serveur
void envoyer_commande(int socket, const char *commande) {
    printf("(sock=%d) Envoi de la commande: \"%s\"...\n", socket, commande);
    if (send(socket, commande, strlen(commande), 0) < 0) {
        perror("Échec de l'envoi");
        fermer_socket(socket);
        exit(EXIT_FAILURE);
    } else {
        printf("(sock=%d) Commande envoyée !\n", socket);
    }
}

// Méthode permettant de recevoir une réponse du serveur et la retourner
char *recevoir_reponse(int socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    printf("(sock=%d) Attente de la réponse du serveur...\n", socket);
    if ((bytes_received = recv(socket, buffer, BUFFER_SIZE, 0)) < 0) {
        perror("Échec de la réception");
        fermer_socket(socket);
        exit(EXIT_FAILURE);
    } else if (bytes_received == 0) {
        printf("(sock=%d) Le serveur a fermé la connexion\n", socket);
        fermer_socket(socket);
        exit(EXIT_FAILURE);
    } else {
        printf("(sock=%d) Réponse reçue !\n", socket);
    }

    buffer[bytes_received] = '\0';
    printf("(sock=%d) Réponse du serveur: \"%s\"\n", socket, buffer);

    return strdup(buffer);
}

// Méthode permettant de faire une demande de libération de ressource au serveur
void liberer_ressource(int socket, int taille) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "RELEASE %d", taille);
    envoyer_commande(socket, buffer);
    char *reponse = recevoir_reponse(socket);
    int ressource_liberee;
    if (sscanf(reponse, "RELEASED %d", &ressource_liberee) == 1) {
        printf("Ressource libérée: %d\n", ressource_liberee);
        total_resources -= ressource_liberee;
    }
}

// Méthode permettant de faire une demande de ressource au serveur
void demander_ressource(int socket, int taille) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "REQUEST %d", taille);
    envoyer_commande(socket, buffer);
    char *reponse = recevoir_reponse(socket);
    int ressource_allouee;
    char erreur[BUFFER_SIZE];
    if (sscanf(reponse, "GRANTED %d", &ressource_allouee) == 1) {
        printf("Ressource allouée: %d\n", ressource_allouee);
        total_resources += ressource_allouee;
    } else if (sscanf(reponse, "DENIED %d, REASON: %[^\n]", &ressource_allouee, erreur) == 2) {
        printf("Ressource refusée: %d, raison: %s\n", ressource_allouee, erreur);

        // Dans le cas où la ressource est refusée, on fait une demande de libération de ressource
        if (total_resources > 0) {
            liberer_ressource(socket, taille);
        }
    }

    // Libérer la mémoire allouée pour la réponse
    free(reponse);
}

// Méthode principale du programme
int main(int argc, char *argv[]) {
    if (argc != 5) {
        usage(argv[0]);
    }

    const char *server_address = argv[1];
    int server_port = atoi(argv[2]);
    int resource_amount = atoi(argv[3]);
    int delay = atoi(argv[4]);

    int sock;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Créer une socket
    sock = socket_client(server_address, server_port);

    for (;;) {
        // Envoyer une demande de ressource au serveur
        demander_ressource(sock, resource_amount);

        // Afficher le total des ressources allouées
        printf("Total des ressources allouées: %d\n", total_resources);

        // Attendre le délai spécifié avant d'envoyer la prochaine demande
        printf("Attente de %d secondes avant la prochaine demande...\n", delay);
        sleep(delay);
    }

    // Fermer la socket
    fermer_socket(sock);
    return 0;
}
