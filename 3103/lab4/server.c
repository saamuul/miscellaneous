
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 5000
#define MAX_CLIENTS 3

sem_t client_slots;

typedef struct {
    int socket_fd;
} ClientArgs;

/* Read one newline-terminated message from a socket. */
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

void *handle_client(void *arg)
{
    ClientArgs *args = (ClientArgs *)arg;
    int client_fd = args->socket_fd;
    char buffer[128];

    free(args);

    if (read_line(client_fd, buffer, sizeof(buffer)) > 0) {
        char *end;
        errno = 0;
        long number = strtol(buffer, &end, 10);

        while (*end == ' ' || *end == '\t')
            end++;

        if (errno == ERANGE || end == buffer || *end != '\0' ||
            number < INT_MIN / 5 || number > INT_MAX / 5) {
            const char *error_msg = "Invalid integer\n";
            send(client_fd, error_msg, strlen(error_msg), 0);
        } else {
            int result = (int)(number * 5);
            char reply[128];

            snprintf(reply, sizeof(reply), "%d\n", result);
            send(client_fd, reply, strlen(reply), 0);

            printf("Received %ld, sent %d\n", number, result);
            fflush(stdout);
        }
    }

    close(client_fd);

    /* Allow another client to be served. */
    sem_post(&client_slots);

    return NULL;
}

int main(void)
{
    int server_fd;
    struct sockaddr_in server_addr;

    if (sem_init(&client_slots, 0, MAX_CLIENTS) != 0) {
        perror("sem_init");
        return 1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        sem_destroy(&client_slots);
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        sem_destroy(&client_slots);
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        sem_destroy(&client_slots);
        return 1;
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        /* Wait if three client workers are already active. */
        while (sem_wait(&client_slots) < 0) {
            if (errno != EINTR) {
                perror("sem_wait");
                close(server_fd);
                sem_destroy(&client_slots);
                return 1;
            }
        }

        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd < 0) {
            perror("accept");
            sem_post(&client_slots);
            continue;
        }

        ClientArgs *args = malloc(sizeof(ClientArgs));
        if (args == NULL) {
            perror("malloc");
            close(client_fd);
            sem_post(&client_slots);
            continue;
        }

        args->socket_fd = client_fd;

        pthread_t tid;
        int rc = pthread_create(&tid, NULL, handle_client, args);

        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            free(args);
            close(client_fd);
            sem_post(&client_slots);
            continue;
        }

        /* Worker threads clean up themselves; no join is needed. */
        pthread_detach(tid);
    }

    close(server_fd);
    sem_destroy(&client_slots);
    return 0;
}
