#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 12345
#define MAX_CLIENTS 10
#define BUFFER_SIZE 256

int available_resources;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int n;
    while ((n = read(client_socket, buffer, BUFFER_SIZE-1)) > 0) {
        buffer[n] = '\0';
        int requested_resources = atoi(buffer);
        if (requested_resources <= 0) {
            close(client_socket);
            return;
        }
        
        if (requested_resources <= available_resources) {
            available_resources -= requested_resources;
            sprintf(buffer, "Resources allocated: %d. Remaining: %d\n", requested_resources, available_resources);
        } else {
            sprintf(buffer, "Not enough resources. Remaining: %d\n", available_resources);
        }
        write(client_socket, buffer, strlen(buffer));
    }
    close(client_socket);
}

void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    socklen_t client_len;
    struct sockaddr_in server_addr, client_addr;
    struct sigaction sa;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <total_resources>\n", argv[0]);
        exit(1);
    }

    available_resources = atoi(argv[1]);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        error("ERROR opening socket");
    }

    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error("ERROR on binding");
    }

    listen(server_socket, MAX_CLIENTS);

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        error("sigaction");
    }

    printf("Server running on port %d with %d total resources\n", PORT, available_resources);

    while (1) {
        client_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            error("ERROR on accept");
        }

        if (fork() == 0) {
            close(server_socket);
            handle_client(client_socket);
            exit(0);
        } else {
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}
