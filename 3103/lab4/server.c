/*
 * server.c  -  Shared circular buffer server (runs on PC)
 *
 * - Listens on a TCP port
 * - Creates one thread per client connection
 * - Shared state: circular buffer (size 5), write index, running sum
 * - Each thread: receives an int, enters critical section (mutex),
 *   inserts value, updates index + sum, replies with: value, slot, sum
 *
 * Build: gcc server.c -o server -pthread
 * Run:   ./server [port]        (default port 8080)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE   5
#define DEFAULT_PORT  8080
#define BACKLOG       16

/* ---------- Shared state (protected by buf_mutex) ---------- */
static int32_t         buffer[BUFFER_SIZE] = {0};
static int             write_index = 0;
static int32_t         buffer_sum  = 0;   /* sum of values currently in buffer */
static pthread_mutex_t buf_mutex   = PTHREAD_MUTEX_INITIALIZER;

/* ---------- Helpers: read/write exactly n bytes ---------- */
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

/* ---------- Per-client thread ---------- */
static void *client_thread(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    /* 1. Receive one integer (network byte order) */
    uint32_t net_val;
    if (read_full(client_fd, &net_val, sizeof(net_val)) < 0) {
        perror("recv");
        close(client_fd);
        return NULL;
    }
    int32_t value = (int32_t)ntohl(net_val);

    /* 2. Critical section */
    pthread_mutex_lock(&buf_mutex);

    int slot = write_index;
    buffer_sum -= buffer[slot];          /* remove old value being overwritten */
    buffer[slot] = value;                /* insert new value                   */
    buffer_sum += value;                 /* add new value to sum               */
    write_index = (write_index + 1) % BUFFER_SIZE;
    int32_t sum_snapshot = buffer_sum;

    pthread_mutex_unlock(&buf_mutex);

    printf("[server] thread %lu: value=%d -> slot=%d, sum=%d\n",
           (unsigned long)pthread_self(), value, slot, sum_snapshot);

    /* 3. Reply: value, slot index, buffer sum */
    uint32_t reply[3];
    reply[0] = htonl((uint32_t)value);
    reply[1] = htonl((uint32_t)slot);
    reply[2] = htonl((uint32_t)sum_snapshot);

    if (write_full(client_fd, reply, sizeof(reply)) < 0)
        perror("send");

    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }
    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("[server] listening on port %d (buffer size %d)\n", port, BUFFER_SIZE);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);

        int *client_fd = malloc(sizeof(int));   /* heap copy so each thread gets its own */
        if (!client_fd) { perror("malloc"); continue; }

        *client_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, client_fd) != 0) {
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
            continue;
        }
        pthread_detach(tid);   /* no need to join; thread cleans itself up */
    }

    close(listen_fd);
    return 0;
}
