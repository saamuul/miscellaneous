/*
 * Server.c  (runs on the RPi)
 *
 * - Port is given on the command line
 * - Runs indefinitely
 * - One thread per client, max 3 threads at a time (semaphore)
 * - Waits a few seconds before replying (simulated delay)
 * - Replies with the received number multiplied by 5
 *
 * Compile: gcc server.c -o server -pthread
 * Run:     ./server 8080
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_THREADS 3      /* max clients served at the same time */
#define DELAY_SEC   3      /* delay before replying to a client   */

sem_t slots;               /* counting semaphore: free thread slots */

void error(char *m)
{
    perror(m);
    exit(1);
}

/* Thread function: serves ONE client */
void *serve_client(void *arg)
{
    int newsockfd = *(int *)arg;
    int n;
    long num;
    char buffer[256];

    free(arg);
    bzero(buffer, 256);

    n = read(newsockfd, buffer, 255);
    if (n < 0) {
        perror("ERROR reading from socket");
    } else {
        num = atol(buffer);
        printf("[thread %lu] Message received: %ld, replying in %d s...\n",
               (unsigned long)pthread_self(), num, DELAY_SEC);

        sleep(DELAY_SEC);                       /* delay the response */

        bzero(buffer, 256);
        sprintf(buffer, "%ld x 5 = %ld", num, num * 5);

        n = write(newsockfd, buffer, strlen(buffer) + 1);
        if (n < 0)
            perror("ERROR writing back to socket");
        else
            printf("[thread %lu] Replied: %s\n",
                   (unsigned long)pthread_self(), buffer);
    }

    close(newsockfd);
    sem_post(&slots);                           /* give the slot back */
    return NULL;
}

int main(int argc, char *argv[])
{
    int sockfd, *newsockfd, port;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t tid;

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\nusage: %s [port]\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);

    signal(SIGPIPE, SIG_IGN);                   /* ignore early client disconnects */

    /* semaphore starts at MAX_THREADS = number of free slots */
    if (sem_init(&slots, 0, MAX_THREADS) != 0)
        error("ERROR initialising semaphore");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);           /* host to network */

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR binding to socket");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    printf("Server listening on port %d (max %d threads, %d s delay)\n",
           port, MAX_THREADS, DELAY_SEC);

    while (1) {                                 /* run indefinitely */
        sem_wait(&slots);                       /* wait for a free slot; if all
                                                   3 are busy, clients queue up */

        newsockfd = malloc(sizeof(int));
        if (newsockfd == NULL) {
            perror("ERROR on malloc");
            sem_post(&slots);
            continue;
        }

        *newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (*newsockfd < 0) {
            perror("ERROR on accept");
            free(newsockfd);
            sem_post(&slots);
            continue;
        }

        printf("Connection from %s:%d\n",
               inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        if (pthread_create(&tid, NULL, serve_client, newsockfd) != 0) {
            perror("ERROR creating thread");
            close(*newsockfd);
            free(newsockfd);
            sem_post(&slots);
            continue;
        }
        pthread_detach(tid);
    }

    return 0;
}
