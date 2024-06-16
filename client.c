#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <server_ip> <server_port> <resource_amount> <delay>\n", prog_name);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        usage(argv[0]);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int resource_amount = atoi(argv[3]);
    int delay = atoi(argv[4]);

    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Créer une socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Échec de la création de la socket");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Adresse invalide / Adresse non supportée");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connexion au serveur
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Échec de la connexion");
        close(sock);
        exit(EXIT_FAILURE);
    }

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
