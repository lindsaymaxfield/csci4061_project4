#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

int keep_going = 1;
const char *serve_dir;

void handle_sigint(int signo) {
    keep_going = 0;
}

// function for worker threads to perform
// reads request and writes response
void *worker_thread(void *arg) {
    connection_queue_t *queue = arg;
    char resource_name[BUFSIZE];
    char resource_path[BUFSIZE];

    while (1) {
        int fd = connection_queue_dequeue(queue);
        if (fd == -1 && queue->shutdown) {
            // exit if file descriptor is invalid and queue has shutdown
            break;
        }

        read_http_request(fd, resource_name);

        // TODO: COPIED FROM PART 1: DOUBLE CHECK
        strcpy(resource_path, serve_dir);
        strcat(resource_path, resource_name);

        write_http_response(fd, resource_path);

        close(fd);    // TODO: error check
    }

    return NULL;
}

int main(int argc, char **argv) {
    // First argument is directory to serve, second is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }
    // Uncomment the lines below to use these definitions:
    serve_dir = argv[1];
    const char *port = argv[2];

    // TODO Implement the rest of this function

    sigset_t main_mask;      // set that stores current signal mask
    sigset_t worker_mask;    // set used to block signals to worker threads
    sigfillset(&worker_mask);
    sigprocmask(SIG_BLOCK, &worker_mask, &main_mask);    // block all signals to workers

    // Create worker threads
    connection_queue_t queue;
    connection_queue_init(&queue);
    pthread_t threads[N_THREADS];
    for (int i = 0; i < N_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker_thread, &queue);
    }

    // Revert to mask before creating threads
    sigprocmask(SIG_SETMASK, &main_mask, NULL);

    // Catch SIGINT so we can clean up properly
    struct sigaction sigact;
    sigact.sa_handler = handle_sigint;
    sigfillset(&sigact.sa_mask);
    sigact.sa_flags = 0;    // No SA_RESTART
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    // TODO: TAKEN FROM PART 1; DOUBLE CHECK

    // Setup TCP Server
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_flags = AI_PASSIVE;        // Will be acting as a server

    struct addrinfo *server;
    int ret_val = getaddrinfo(NULL, port, &hints, &server);
    if (ret_val) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret_val));
    }
    int sock_fd =
        socket(server->ai_family, server->ai_socktype, server->ai_protocol);    // TODO: error check
    if (sock_fd == -1) {
        perror("socket");
        freeaddrinfo(server);
        freeaddrinfo(&hints);
        return 1;
    }
    if (bind(sock_fd, server->ai_addr, server->ai_addrlen)) {
        perror("bind");
        return 1;
    }
    if (listen(sock_fd, LISTEN_QUEUE_LEN)) {
        perror("listen");
        freeaddrinfo(server);
        close(sock_fd);
        return 1;
    }
    freeaddrinfo(server);
    freeaddrinfo(&hints);

    // Main thread loop
    while (keep_going) {
        int client_fd = accept(sock_fd, NULL, NULL);
        connection_queue_enqueue(&queue, client_fd);
    }

    // Once SIGINT has been sent
    connection_queue_shutdown(&queue);
    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    connection_queue_free(&queue);
    close(sock_fd);

    return 0;
}
