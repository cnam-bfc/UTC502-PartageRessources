#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <server_address> <server_port> <resource_amount> <delay>\n", prog_name);
    exit(EXIT_FAILURE);
}

int socket_client(const char *address, unsigned short port) {
    int client_socket;
    struct sockaddr_in serveur_sockaddr_in;
    struct hostent *hostent;

    if ((hostent = gethostbyname(address)) == NULL) {
        perror("Erreur lors de l'appel de gethostbyname()");
        exit(EXIT_FAILURE);
    }

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Erreur lors de l'appel de socket()");
        exit(EXIT_FAILURE);
    }

    memset(&serveur_sockaddr_in, 0, sizeof(serveur_sockaddr_in));
    serveur_sockaddr_in.sin_family = AF_INET;
    serveur_sockaddr_in.sin_port = htons(port);
    memcpy(&serveur_sockaddr_in.sin_addr, hostent->h_addr_list[0], hostent->h_length);

    printf("Connexion à %s (%s) sur le port %d...\n", hostent->h_name, address, port);
    if (connect(client_socket, (struct sockaddr*)&serveur_sockaddr_in, sizeof(serveur_sockaddr_in)) == -1) {
        perror("Erreur lors de l'appel de connect()");
        exit(EXIT_FAILURE);
    } else {
        printf("Connecté !\n");
    }
    return client_socket;
}

void fermer_socket(int socket) {
    printf("Fermeture de la socket...\n");
    if (close(socket) == -1) {
        perror("Erreur lors de la fermeture de la socket");
        exit(EXIT_FAILURE);
    } else {
        printf("Socket fermée !\n");
    }
}

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

    while (1) {
        // Envoyer une demande de ressource au serveur
        snprintf(buffer, BUFFER_SIZE, "REQUEST %d", resource_amount);
        printf("Envoi de la demande: \"%s\"...\n", buffer);
        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("Échec de l'envoi");
            fermer_socket(sock);
            exit(EXIT_FAILURE);
        } else {
            printf("Demande envoyée !\n");
        }

        // Recevoir la réponse du serveur
        printf("Attente de la réponse du serveur...\n");
        if ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) < 0) {
            perror("Échec de la réception");
            fermer_socket(sock);
            exit(EXIT_FAILURE);
        } else if (bytes_received == 0) {
            printf("Le serveur a fermé la connexion\n");
            fermer_socket(sock);
            exit(EXIT_FAILURE);
        } else {
            printf("Réponse reçue !\n");
        }

        buffer[bytes_received] = '\0';
        printf("Réponse du serveur: \"%s\"\n", buffer);

        // Traiter la réponse du serveur
        if (sscanf(buffer, "GRANTED %d", &resource_amount) == 1) {
            printf("Ressource allouée: %d\n", resource_amount);
        } else if (sscanf(buffer, "DENIED %d", &resource_amount) == 1) {
            printf("Ressource refusée: %d\n", resource_amount);
        }

        // Attendre le délai spécifié avant d'envoyer la prochaine demande
        printf("Attente de %d secondes avant la prochaine demande...\n", delay);
        sleep(delay);
    }

    // Fermer la socket
    fermer_socket(sock);
    return 0;
}
