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
int sock_fd = -1;
const char *serve_dir;

// Coordination to stop curl (7): Connection refused on startup
pthread_mutex_t ready_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready_cond = PTHREAD_COND_INITIALIZER;
int ready_count = 0;    // waits until all 5 threads are ready to enter their loops

/**
 * @brief Handler to shutdown server on SIGINT
 *
 * @details sets global variable "keep_going" to 0 which breaks out of main accept() loop
 */
void handle_sigint(int signo) {
    keep_going = 0;
}

/**
 * @brief Worker thread function to parse http requests and send http responses
 *
 * @details Continually loops to get file descriptors from a shared (thread-safe) queue, reads the
 * http request, finds the request resource, and writes an http response back
 *
 * @param arg should be a connection_queue_t pointer which stores client fds in a thread-safe manner
 */
void *worker_thread(void *arg) {
    connection_queue_t *queue = (connection_queue_t *) arg;
    char resource_name[BUFSIZE];
    char resource_path[BUFSIZE];
    // int read_error = 0;

    pthread_mutex_lock(&ready_lock);
    ready_count++;
    pthread_cond_signal(&ready_cond);
    pthread_mutex_unlock(&ready_lock);

    while (1) {
        if (ready_count == N_THREADS) {
            break;
        }
    }

    while (keep_going) {
        int fd = connection_queue_dequeue(queue);
        if (fd == -1) {
            // exit if file descriptor is invalid and queue has shutdown
            break;
        }

        if (read_http_request(fd, resource_name)) {
            // if (!queue->shutdown) {
            printf("Error from reading in worker thread\n");
            //}
            close(fd);
            break;
        }

        // if (!read_error) {
        snprintf(resource_path, (strlen(serve_dir) + strlen(resource_name) + 1), "%s%s", serve_dir,
                 resource_name);
        //}

        if (write_http_response(fd, resource_path)) {
            printf("Error from writing in worker thread\n");
            close(fd);
            break;
        }

        close(fd);    // TODO: error check
    }

    return NULL;
}

/**
 * @brief Helper function to cleanup threads on error
 *
 * @details iterates from start_val to end_val-1 and joins the thread at that index in threads.
 * Calling join_multiple_threads(0, N_THREADS,...) will join all threads
 *
 * @param start_val initial index of thread to start joining (inclusive)
 * @param end_val final index of iteration (exclusive)
 */
void join_multiple_threads(int start_val, int end_val, pthread_t threads[N_THREADS]) {
    for (int j = start_val; j < end_val; j++) {
        // pthread_join() not error checked because this function is only called during the cleanup
        // of another error Final join loop only uses this function in the error case
        pthread_join(threads[j], NULL);
    }
}

int main(int argc, char **argv) {
    // First argument is directory to serve, second is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }

    serve_dir = argv[1];
    const char *port = argv[2];

    // Create worker threads
    connection_queue_t queue;
    connection_queue_init(&queue);

    // Catch SIGINT so we can clean up properly
    struct sigaction sigact;
    sigact.sa_handler = handle_sigint;
    if (sigfillset(&sigact.sa_mask) == -1) {
        perror("sigfillset");
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        return 1;
    }
    sigact.sa_flags = 0;    // No SA_RESTART
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        return 1;
    }

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
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        return 1;
    }
    sock_fd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sock_fd == -1) {
        perror("socket");
        freeaddrinfo(server);
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        return 1;
    }
    if (bind(sock_fd, server->ai_addr, server->ai_addrlen)) {
        perror("bind");
        freeaddrinfo(server);
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        close(sock_fd);
        return 1;
    }
    if (listen(sock_fd, LISTEN_QUEUE_LEN)) {
        perror("listen");
        freeaddrinfo(server);
        connection_queue_shutdown(&queue);
        connection_queue_free(&queue);
        close(sock_fd);
        return 1;
    }
    freeaddrinfo(server);

    // Signal handling
    sigset_t main_mask;      // set that stores current signal mask
    sigset_t worker_mask;    // set used to block signals to worker threads
    if (sigfillset(&worker_mask)) {
        perror("sigfillset");
        return 1;
    }
    // block all signals to workers
    if (sigprocmask(SIG_BLOCK, &worker_mask, &main_mask)) {
        perror("sigprocmask");
        return 1;
    }

    pthread_t threads[N_THREADS];
    for (int i = 0; i < N_THREADS; i++) {
        int ret_val = pthread_create(&threads[i], NULL, worker_thread, &queue);
        if (ret_val != 0) {
            fprintf(stderr, "error creating thread number %d: %s", i, strerror(ret_val));
            connection_queue_shutdown(&queue);
            join_multiple_threads(0, i, threads);
            connection_queue_free(&queue);
            return 1;
        }
    }

    // Revert to mask from before creating threads, so main thread can receive signals
    if (sigprocmask(SIG_SETMASK, &main_mask, NULL)) {
        perror("sigprocmask");
        connection_queue_shutdown(&queue);
        join_multiple_threads(0, N_THREADS, threads);
        connection_queue_free(&queue);
        return 1;
    }

    pthread_mutex_lock(&ready_lock);
    while (ready_count < N_THREADS)
        pthread_cond_wait(&ready_cond, &ready_lock);
    pthread_mutex_unlock(&ready_lock);

    // Main thread loop
    while (keep_going) {
        int client_fd = accept(sock_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                break;
            }
            perror("accept");
            connection_queue_shutdown(&queue);
            join_multiple_threads(0, N_THREADS, threads);
            connection_queue_free(&queue);
            close(sock_fd);
            return 1;
        }
        if (connection_queue_enqueue(&queue, client_fd)) {
            printf("Enqueue error\n");
            connection_queue_shutdown(&queue);
            join_multiple_threads(0, N_THREADS, threads);
            connection_queue_free(&queue);
            // close(client_fd);
            close(sock_fd);
            return 1;
        }
    }

    // Once SIGINT has been sent
    connection_queue_shutdown(&queue);    // TODO error check
    close(sock_fd);                       // TODO error check
    for (int i = 0; i < N_THREADS; i++) {
        int result = pthread_join(threads[i], NULL);
        if (result != 0) {
            fprintf(stderr, "pthread_join failed: %s\n", strerror(result));
            join_multiple_threads(i + 1, N_THREADS, threads);
            connection_queue_free(&queue);
            return 1;
        }
    }
    connection_queue_free(&queue);    // TODO error check

    return 0;
}
