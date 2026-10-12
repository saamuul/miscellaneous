#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define NUM_CHILDREN 3

/* Parse a port number (1-65535). Returns -1 if invalid. */
static int parse_port(const char *s)
{
    char *end;
    long value;

    errno = 0;
    value = strtol(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0' ||
        value < 1 || value > 65535)
        return -1;

    return (int)value;
}

int read_line(int fd, char *buf, size_t size)
{
    size_t i = 0;

    while (i < size - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);

        if (n == 0)
            break;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (c == '\n')
            break;

        buf[i++] = c;
    }

    buf[i] = '\0';
    return (int)i;
}

void run_child(int child_number, const struct sockaddr_in *server_addr)
{
    int sock;
    char message[64], reply[128];

    /* Give each child a different number. */
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
    int number = child_number * 100 + rand() % 100;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    if (connect(sock, (const struct sockaddr *)server_addr,
                sizeof(*server_addr)) < 0) {
        perror("connect");
        close(sock);
        exit(1);
    }

    int len = snprintf(message, sizeof(message),
                       "%d\n", number);

    size_t sent = 0;
    while (sent < (size_t)len) {
        ssize_t n = send(sock, message + sent,
                         (size_t)len - sent, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("send");
            close(sock);
            exit(1);
        }
        sent += (size_t)n;
    }

    int n = read_line(sock, reply, sizeof(reply));

    if (n > 0) {
        printf("Child %d (PID %ld): sent %d, received %s\n",
               child_number, (long)getpid(), number, reply);
    } else {
        printf("Child %d: no response\n", child_number);
    }

    close(sock);
    exit(n > 0 ? 0 : 1);
}

int main(int argc, char *argv[])
{
    pid_t children[NUM_CHILDREN] = {0};
    struct sockaddr_in server_addr;
    int port;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    port = parse_port(argv[2]);
    if (port < 0) {
        fprintf(stderr, "Invalid port '%s' (must be 1-65535)\n", argv[2]);
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server IP '%s'\n", argv[1]);
        return 1;
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            break;
        }

        if (pid == 0) {
            run_child(i + 1, &server_addr);
        }

        children[i] = pid;
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (children[i] > 0) {
            waitpid(children[i], NULL, 0);
        }
    }

    printf("All child processes completed.\n");
    return 0;
}
