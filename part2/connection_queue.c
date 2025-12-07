#include "connection_queue.h"

#include <stdio.h>
#include <string.h>

int connection_queue_init(connection_queue_t *queue) {
    // TODO: Check if malloc needed here
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->length = 0;
    queue->shutdown = 0;

    for (int i = 0; i < CAPACITY; ++i)
        queue->client_fds[i] = -1;

    pthread_mutex_init(&queue->lock, NULL);    // mutex_init does not return errors

    int error_code = 0;
    if ((error_code = pthread_cond_init(&queue->queue_not_full, NULL))) {
        fprintf(stderr, "pthread_cond_init: %s\n", strerror(error_code));
        pthread_mutex_destroy(&queue->lock);
        return -1;
    }
    if ((error_code = pthread_cond_init(&queue->queue_not_empty, NULL))) {
        fprintf(stderr, "pthread_cond_init: %s\n", strerror(error_code));
        pthread_mutex_destroy(&queue->lock);
        pthread_cond_destroy(&queue->queue_not_full);
        return -1;
    }

    return 0;
}

int connection_queue_enqueue(connection_queue_t *queue, int connection_fd) {
    // TODO Implement Shutdown
    int error_code = 0;
    error_code = pthread_mutex_lock(&queue->lock);
    if (error_code) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(error_code));
        // Clean up handled by caller in htpp_server
        return -1;
    }

    while (queue->length == CAPACITY && !queue->shutdown) {
        error_code = pthread_cond_wait(&queue->queue_not_full, &queue->lock);
        if (error_code) {
            fprintf(stderr, "pthread_cond_wait: %s\n", strerror(error_code));
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }

    if (queue->shutdown) {
        error_code = pthread_mutex_unlock(&queue->lock);
        if (error_code) {
            fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(error_code));
        }
        return -1;
    }

    queue->client_fds[queue->write_idx] = connection_fd;
    queue->write_idx = (queue->write_idx + 1) % CAPACITY;
    queue->length++;

    error_code = pthread_cond_signal(&queue->queue_not_empty);
    if (error_code) {
        fprintf(stderr, "pthread_cond_signal: %s\n", strerror(error_code));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    error_code = pthread_mutex_unlock(&queue->lock);
    if (error_code) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(error_code));
        return -1;
    }

    return 0;
}

int connection_queue_dequeue(connection_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);

    while (queue->length == 0 && !queue->shutdown)
        pthread_cond_wait(&queue->queue_not_empty, &queue->lock);

    // Only exit if no work to do
    if (queue->length == 0 && queue->shutdown) {
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    int fd = queue->client_fds[queue->read_idx];
    queue->read_idx = (queue->read_idx + 1) % CAPACITY;
    queue->length--;

    pthread_cond_signal(&queue->queue_not_full);
    pthread_mutex_unlock(&queue->lock);
    return fd;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    // TODO Not yet implemented
    pthread_mutex_lock(&queue->lock);
    queue->shutdown = 1;
    // send both signals to wake up all blocked threads
    pthread_cond_broadcast(&queue->queue_not_full);
    pthread_cond_broadcast(&queue->queue_not_empty);
    pthread_mutex_unlock(&queue->lock);

    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    // TODO Not yet implemented
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->queue_not_empty);
    pthread_cond_destroy(&queue->queue_not_full);

    return 0;
}
