#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

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

int main(int argc, char *argv[])
{
    int sock;
    int port;
    struct sockaddr_in server_addr;
    char input[128];
    char reply[128];
    char *end;
    long number;

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
        fprintf(stderr, "Invalid server IP address '%s'\n", argv[1]);
        return 1;
    }

    printf("Enter an integer: ");
    fflush(stdout);

    if (fgets(input, sizeof(input), stdin) == NULL)
        return 1;

    errno = 0;
    number = strtol(input, &end, 10);

    while (*end == ' ' || *end == '\t' ||
           *end == '\n' || *end == '\r')
        end++;

    if (errno == ERANGE || end == input || *end != '\0') {
        printf("Invalid integer\n");
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    char message[128];
    int len = snprintf(message, sizeof(message),
                       "%ld\n", number);

    if (len < 0 || len >= (int)sizeof(message)) {
        fprintf(stderr, "Message too long\n");
        close(sock);
        return 1;
    }

    size_t sent = 0;
    while (sent < (size_t)len) {
        ssize_t n = send(sock, message + sent,
                         (size_t)len - sent, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("send");
            close(sock);
            return 1;
        }
        sent += (size_t)n;
    }

    int n = read_line(sock, reply, sizeof(reply));

    if (n > 0)
        printf("Server response: %s\n", reply);
    else
        printf("No response received from server\n");

    close(sock);
    return 0;
}
