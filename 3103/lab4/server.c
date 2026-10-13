/*
 * server.c  -  Multithreaded multiply-by-5 server (runs on the RPi)
 *
 * - Listens on a TCP port given on the command line
 * - Runs indefinitely
 * - Spawns one thread per client, but never more than MAX_THREADS (3)
 *   at once, enforced with a counting semaphore
 * - Each thread waits REPLY_DELAY_SEC seconds before replying (simulated
 *   work) so that concurrent clients and the thread limit are visible
 * - Reply to the client: the number received multiplied by 5
 *
 * Build: gcc server.c -o server -pthread
 * Run:   ./server <port>        e.g. ./server 8080
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_THREADS      3   /* max clients served at the same time */
#define REPLY_DELAY_SEC  3   /* delay before answering a client     */
#define BACKLOG          10
#define MSG_SIZE         128

/* Counts free "thread slots". Starts at MAX_THREADS.
 * main()   : sem_wait before accepting a client  (take a slot)
 * a thread : sem_post when finished              (give the slot back) */
static sem_t slots;

static void *client_thread(void *arg)
{
    int  fd = *(int *)arg;
    free(arg);

    char    buf[MSG_SIZE];
    char    reply[MSG_SIZE];
    int     free_slots;

    sem_getvalue(&slots, &free_slots);
    printf("[thread %lu] started (active threads: %d/%d)\n",
           (unsigned long)pthread_self(), MAX_THREADS - free_slots, MAX_THREADS);

    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        printf("[thread %lu] client closed connection without data\n",
               (unsigned long)pthread_self());
        goto done;
    }
    buf[n] = '\0';

    char *end;
    errno = 0;
    long long num = strtoll(buf, &end, 10);

    if (end == buf || errno == ERANGE || num > LLONG_MAX / 5 || num < LLONG_MIN / 5) {
        snprintf(reply, sizeof(reply), "Error: invalid or out-of-range number");
        printf("[thread %lu] bad input received\n", (unsigned long)pthread_self());
    } else {
        printf("[thread %lu] received %lld, processing for %d s...\n",
               (unsigned long)pthread_self(), num, REPLY_DELAY_SEC);

        sleep(REPLY_DELAY_SEC);          /* deliberate delay before replying */

        snprintf(reply, sizeof(reply), "%lld x 5 = %lld", num, num * 5);
        printf("[thread %lu] replying: %s\n", (unsigned long)pthread_self(), reply);
    }

    if (send(fd, reply, strlen(reply), 0) < 0)
        perror("send");

done:
    close(fd);
    sem_post(&slots);                    /* free the slot for the next client */
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s (must be 1-65535)\n", argv[1]);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);            /* don't die if a client disconnects early */

    if (sem_init(&slots, 0, MAX_THREADS) != 0) {
        perror("sem_init");
        return 1;
    }

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

    printf("[server] listening on port %d (max %d concurrent threads, %d s reply delay)\n",
           port, MAX_THREADS, REPLY_DELAY_SEC);

    /* Run indefinitely */
    while (1) {
        /* Wait for a free slot. If all 3 threads are busy, we block here and
         * extra clients simply wait in the kernel's listen queue. */
        sem_wait(&slots);

        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);

        int *client_fd = malloc(sizeof(int));
        if (!client_fd) {
            perror("malloc");
            sem_post(&slots);
            continue;
        }

        *client_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            sem_post(&slots);
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
        printf("[server] accepted connection from %s:%d\n", ip, ntohs(cli_addr.sin_port));

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, client_fd) != 0) {
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
            sem_post(&slots);
            continue;
        }
        pthread_detach(tid);
    }

    /* never reached */
    close(listen_fd);
    sem_destroy(&slots);
    return 0;
}
