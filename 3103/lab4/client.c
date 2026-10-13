/*
 * client.c  -  Client for the multiply-by-5 server (runs on the Ubuntu PC)
 *
 * Two modes:
 *   1) Interactive (Exercise 3):  ./client <server_ip> <port>
 *        Asks the user to key in a number, sends it to the server and
 *        displays the message received from the server.
 *
 *   2) Multi-request (Exercise 4): ./client <server_ip> <port> <N>
 *        fork()s N child processes. Each child generates a random number,
 *        connects to the server, sends it and prints the server's reply.
 *        (The exercise asks for up to 3 children; use N > 3, e.g. 6, to
 *        watch the server's 3-thread limit make the extra clients wait.)
 *
 * Build: gcc client.c -o client
 * Run:   ./client 192.168.1.20 8080
 *        ./client 192.168.1.20 8080 3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MSG_SIZE     128
#define MAX_CHILDREN 10

/* Connect, send one number (as text), receive and print the server's reply.
 * Returns 0 on success, -1 on failure. */
static int send_request(const char *ip, int port, long long num, const char *tag)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &serv.sin_addr) <= 0) {
        fprintf(stderr, "%sInvalid server IP: %s\n", tag, ip);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    char msg[MSG_SIZE];
    snprintf(msg, sizeof(msg), "%lld", num);

    printf("%sSending %lld to server...\n", tag, num);
    if (send(fd, msg, strlen(msg), 0) < 0) {
        perror("send");
        close(fd);
        return -1;
    }

    char reply[MSG_SIZE];
    ssize_t n = recv(fd, reply, sizeof(reply) - 1, 0);   /* blocks until server replies */
    if (n <= 0) {
        fprintf(stderr, "%sNo reply from server\n", tag);
        close(fd);
        return -1;
    }
    reply[n] = '\0';

    printf("%sServer replied: %s\n", tag, reply);
    close(fd);
    return 0;
}

/* Exercise 3: ask the user for a number */
static int run_interactive(const char *ip, int port)
{
    char line[MSG_SIZE];
    char *end;

    printf("Enter a number: ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
        fprintf(stderr, "No input\n");
        return 1;
    }

    errno = 0;
    long long num = strtoll(line, &end, 10);
    if (end == line || errno == ERANGE) {
        fprintf(stderr, "Invalid number\n");
        return 1;
    }

    return send_request(ip, port, num, "") == 0 ? 0 : 1;
}

/* Exercise 4: fork N children, each sends a different random number */
static int run_multi(const char *ip, int port, int n)
{
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            break;
        }
        if (pid == 0) {
            /* ---- child ---- */
            srand((unsigned)time(NULL) ^ ((unsigned)getpid() << 8) ^ (unsigned)i);
            long long num = (rand() % 1000) + 1;      /* 1..1000 */

            char tag[32];
            snprintf(tag, sizeof(tag), "[child %d, PID %d] ", i + 1, getpid());

            int rc = send_request(ip, port, num, tag);
            exit(rc == 0 ? 0 : 1);
        }
        /* ---- parent: loop on to fork the next child ---- */
    }

    while (wait(NULL) > 0)       /* wait for all children */
        ;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr,
                "Usage: %s <server_ip> <port> [N]\n"
                "  no N : interactive mode (type a number)\n"
                "  N    : fork N children, each sending a random number\n",
                argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s (must be 1-65535)\n", argv[2]);
        return 1;
    }

    if (argc == 3)
        return run_interactive(ip, port);

    int n = atoi(argv[3]);
    if (n <= 0 || n > MAX_CHILDREN) {
        fprintf(stderr, "N must be between 1 and %d\n", MAX_CHILDREN);
        return 1;
    }
    return run_multi(ip, port, n);
}
