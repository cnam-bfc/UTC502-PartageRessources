#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

void usage(char *progname) {
    fprintf(stderr, "Usage: %s <server_ip> <request_interval> <requests...>\n", progname);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        usage(argv[0]);
    }

    char *server_ip = argv[1];
    int request_interval = atoi(argv[2]);

    int sock;
    struct sockaddr_in server_addr;
    char buffer[1024];
    int n;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    for (int i = 3; i < argc; ++i) {
        int resource_request = atoi(argv[i]);
        sprintf(buffer, "%d", resource_request);

        send(sock, buffer, strlen(buffer), 0);
        n = recv(sock, buffer, sizeof(buffer), 0);
        buffer[n] = '\0';
        printf("Server: %s\n", buffer);

        sleep(request_interval);
    }

    close(sock);
    return 0;
}
