/*
 * Client.c  (runs on the Ubuntu PC)
 *
 * Usage: ./client [hostname] [port]        -> asks the user to key in a number
 *        ./client [hostname] [port] [N]    -> forks N children, each sends a
 *                                             random number to the server
 *
 * Compile: gcc client.c -o client
 * Run:     ./client 192.168.1.20 8080
 *          ./client 192.168.1.20 8080 3
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(char *m)
{
    perror(m);
    exit(1);
}

/* Connects to the server, sends 'msg', prints the reply.
 * 'tag' is a prefix for the printed lines (e.g. child PID). */
void send_request(char *hostname, int port, char *msg, char *tag)
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(1);
    }

    // start with a clean address structure
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;             // internet socket
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    printf("%sSending: %s\n", tag, msg);
    n = write(sockfd, msg, strlen(msg));
    if (n < 0)
        error("ERROR writing to socket");

    bzero(buffer, 256);
    n = read(sockfd, buffer, 255);              // blocks until server replies
    if (n < 0)
        error("ERROR reading from socket");

    printf("%sServer replied: %s\n", tag, buffer);
    close(sockfd);
}

int main(int argc, char *argv[])
{
    int port, i, nchild;
    pid_t pid;
    char buffer[256];
    char tag[64];

    if (argc < 3)
        error("usage client [hostname] [port] [optional: number of children]\n");

    port = atoi(argv[2]);

    /* ---------- Mode 1: user keys in a number ---------- */
    if (argc == 3) {
        printf("Please enter a number: ");
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';   // strip newline

        send_request(argv[1], port, buffer, "");
        return 0;
    }

    /* ---------- Mode 2: fork N children, random numbers ---------- */
    nchild = atoi(argv[3]);
    if (nchild < 1 || nchild > 10)
        error("N must be between 1 and 10\n");

    for (i = 0; i < nchild; i++) {
        pid = fork();
        if (pid < 0)
            error("ERROR on fork");

        if (pid == 0) {
            /* child process */
            srand(time(NULL) ^ (getpid() << 8) ^ i);
            sprintf(buffer, "%d", (rand() % 1000) + 1);   // random 1..1000
            sprintf(tag, "[child %d, PID %d] ", i + 1, getpid());

            send_request(argv[1], port, buffer, tag);
            exit(0);
        }
        /* parent: carry on and fork the next child */
    }

    while (wait(NULL) > 0)                      // wait for all children
        ;

    return 0;
}
