#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 12345
#define BUFFER_SIZE 256

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int sockfd, n;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <hostname> <resources> <delay>\n", argv[0]);
        exit(1);
    }

    int resources = atoi(argv[2]);
    int delay = atoi(argv[3]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        error("ERROR invalid host");
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR connecting");
    }

    sprintf(buffer, "%d", resources);
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) {
        error("ERROR writing to socket");
    }

    memset(buffer, 0, BUFFER_SIZE);
    n = read(sockfd, buffer, BUFFER_SIZE-1);
    if (n < 0) {
        error("ERROR reading from socket");
    }

    printf("%s\n", buffer);

    close(sockfd);
    sleep(delay);

    return 0;
}
