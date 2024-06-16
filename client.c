#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <server_ip> <server_port> <resource_amount> <delay>\n", prog_name);
    exit(EXIT_FAILURE);
}

int socket_client(const char *serveur, unsigned short port) {
    int client_socket;
    struct sockaddr_in serveur_sockaddr_in;
    struct hostent *hostent;

    if ((hostent = gethostbyname(serveur)) == NULL) {
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

    printf("Connexion a %s (%s) sur le port %d\n", hostent->h_name, serveur, port);
    if (connect(client_socket, (struct sockaddr*)&serveur_sockaddr_in, sizeof(serveur_sockaddr_in)) == -1) {
        perror("Erreur lors de l'appel de connect()");
        exit(EXIT_FAILURE);
    }
    return client_socket;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        usage(argv[0]);
    }

    const char *server_ip = argv[1];
    unsigned short server_port = atoi(argv[2]);
    int resource_amount = atoi(argv[3]);
    int delay = atoi(argv[4]);

    int sock;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Créer une socket
    sock = socket_client(server_ip, server_port);

    while (1) {
        // Envoyer une demande de ressource au serveur
        snprintf(buffer, BUFFER_SIZE, "DEMANDE %d", resource_amount);
        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("Échec de l'envoi");
            close(sock);
            exit(EXIT_FAILURE);
        }

        // Recevoir la réponse du serveur
        if ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) < 0) {
            perror("Échec de la réception");
            close(sock);
            exit(EXIT_FAILURE);
        }

        buffer[bytes_received] = '\0';
        printf("Réponse du serveur: %s\n", buffer);

        // Attendre le délai spécifié avant d'envoyer la prochaine demande
        sleep(delay);
    }

    // Fermer la socket
    close(sock);
    return 0;
}
