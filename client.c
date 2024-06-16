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

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Send resource request to server
        snprintf(buffer, BUFFER_SIZE, "REQUEST %d", resource_amount);
        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("Send failed");
            close(sock);
            exit(EXIT_FAILURE);
        }

        // Receive response from server
        if ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) < 0) {
            perror("Receive failed");
            close(sock);
            exit(EXIT_FAILURE);
        }

        buffer[bytes_received] = '\0';
        printf("Server response: %s\n", buffer);

        // Wait for the specified delay
        sleep(delay);
    }

    // Close socket
    close(sock);
    return 0;
}
