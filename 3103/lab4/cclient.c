/*
 * client.c  -  Shared buffer client (runs on Raspberry Pi)
 *
 * - Takes N on the command line and fork()s N child processes
 * - Each child connects to the server over TCP, sends one integer,
 *   receives a reply (value, slot index, buffer sum) and prints:
 *       PID, value sent, buffer slot index, current buffer sum
 *
 * Build: gcc client.c -o client
 * Run:   ./client <N> <server_ip> <port>
 *        e.g. ./client 8 192.168.1.50 8080
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

static int read_full(int fd, void *buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        ssize_t r = recv(fd, (char *)buf + got, n - got, 0);
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

static int write_full(int fd, const void *buf, size_t n)
{
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = send(fd, (const char *)buf + sent, n - sent, 0);
        if (w <= 0) return -1;
        sent += (size_t)w;
    }
    return 0;
}

/* Work done by each child process */
static void child_work(const char *ip, int port)
{
    pid_t pid = getpid();

    /* Different random value per child (seed mixes time and PID) */
    srand((unsigned)time(NULL) ^ (unsigned)pid);
    int32_t value = (rand() % 100) + 1;      /* 1..100 */

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &serv.sin_addr) <= 0) {
        fprintf(stderr, "[PID %d] invalid server IP: %s\n", pid, ip);
        close(fd);
        exit(1);
    }

    if (connect(fd, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("connect");
        close(fd);
        exit(1);
    }

    /* Send one integer (network byte order) */
    uint32_t net_val = htonl((uint32_t)value);
    if (write_full(fd, &net_val, sizeof(net_val)) < 0) {
        perror("send");
        close(fd);
        exit(1);
    }

    /* Receive reply: value, slot index, buffer sum */
    uint32_t reply[3];
    if (read_full(fd, reply, sizeof(reply)) < 0) {
        fprintf(stderr, "[PID %d] failed to read reply\n", pid);
        close(fd);
        exit(1);
    }

    int32_t r_value = (int32_t)ntohl(reply[0]);
    int32_t r_slot  = (int32_t)ntohl(reply[1]);
    int32_t r_sum   = (int32_t)ntohl(reply[2]);
    (void)r_value;   /* server echoes the value; we print the one we sent */

    printf("PID: %d | Value sent: %d | Buffer slot: %d | Buffer sum: %d\n",
           pid, value, r_slot, r_sum);

    close(fd);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <N> <server_ip> <port>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    if (n <= 0) {
        fprintf(stderr, "N must be a positive integer\n");
        return 1;
    }

    const char *ip = argv[2];

    int port = atoi(argv[3]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s (must be 1-65535)\n", argv[3]);
        return 1;
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            break;
        } else if (pid == 0) {
            child_work(ip, port);   /* never returns */
        }
        /* parent keeps looping to spawn the next child */
    }

    /* Parent waits for all children to finish */
    while (wait(NULL) > 0)
        ;

    return 0;
}
