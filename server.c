#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 10

int available_resource;
pthread_mutex_t resource_lock;

typedef struct {
    struct sockaddr_in address;
    int sock;
    int index;
    int resource_consumed;
} client_t;

client_t *clients[MAX_CLIENTS];

void *handle_client(void *arg) {
    client_t *cli = (client_t *)arg;
    char buffer[1024];
    int n;

    while ((n = recv(cli->sock, buffer, sizeof(buffer), 0)) > 0) {
        buffer[n] = '\0';
        int requested_resource;
        sscanf(buffer, "%d", &requested_resource);

        pthread_mutex_lock(&resource_lock);

        if (requested_resource > 0) {
            if (available_resource >= requested_resource) {
                available_resource -= requested_resource;
                cli->resource_consumed += requested_resource;
                sprintf(buffer, "Resource allocated. Available: %d", available_resource);
            } else {
                sprintf(buffer, "Not enough resources. Available: %d", available_resource);
            }
        } else {
            available_resource += (-requested_resource);
            cli->resource_consumed += requested_resource;
            sprintf(buffer, "Resource released. Available: %d", available_resource);
        }

        pthread_mutex_unlock(&resource_lock);
        send(cli->sock, buffer, strlen(buffer), 0);
    }

    pthread_mutex_lock(&resource_lock);
    available_resource += cli->resource_consumed;
    close(cli->sock);
    clients[cli->index] = NULL;
    free(cli);
    pthread_mutex_unlock(&resource_lock);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <initial_resource>\n", argv[0]);
        return 1;
    }

    available_resource = atoi(argv[1]);
    int server_sock, new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    pthread_t tid;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_sock);
        return 1;
    }

    printf("Server started on port %d with %d resources.\n", PORT, available_resource);

    while (1) {
        addr_size = sizeof(client_addr);
        new_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (new_sock < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&resource_lock);

        int i;
        for (i = 0; i < MAX_CLIENTS; ++i) {
            if (!clients[i]) {
                client_t *cli = (client_t *)malloc(sizeof(client_t));
                cli->address = client_addr;
                cli->sock = new_sock;
                cli->index = i;
                cli->resource_consumed = 0;
                clients[i] = cli;
                pthread_create(&tid, NULL, handle_client, (void *)cli);
                break;
            }
        }

        if (i == MAX_CLIENTS) {
            printf("Max clients reached. Connection rejected: %s:%d\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            close(new_sock);
        }

        pthread_mutex_unlock(&resource_lock);
    }

    close(server_sock);
    return 0;
}
