#include "connection_queue.h"

#include <stdio.h>
#include <string.h>

int connection_queue_init(connection_queue_t *queue) {
    // TODO: Check if malloc needed here
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->length = 0;
    queue->shutdown = 0;

    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->queue_not_full, NULL);
    pthread_cond_init(&queue->queue_not_empty, NULL);

    return 0;
}

int connection_queue_enqueue(connection_queue_t *queue, int connection_fd) {
    // TODO Implement Shutdown
    pthread_mutex_lock(&queue->lock);

    while (queue->length == CAPACITY) {
        pthread_cond_wait(&queue->queue_not_full, &queue->lock);
    }

    queue->client_fds[queue->write_idx] = connection_fd;
    queue->write_idx = (queue->write_idx + 1) % CAPACITY;
    queue->length++;
    // If this is the first thing added to the queue, make sure read_idx is valid
    if (queue->read_idx == -1) {
        queue->read_idx = 0;
    }

    pthread_cond_signal(&queue->queue_not_empty);
    pthread_mutex_unlock(&queue->lock);

    return 0;
}

int connection_queue_dequeue(connection_queue_t *queue) {
    // TODO: Implement shutdown
    pthread_mutex_lock(&queue->lock);

    while (queue->length == 0) {
        pthread_cond_wait(&queue->queue_not_empty, &queue->lock);
    }

    int retval = queue->client_fds[queue->read_idx];
    queue->length--;
    queue->read_idx = (queue->read_idx + 1) % CAPACITY;

    pthread_cond_signal(&queue->queue_not_full);
    pthread_mutex_unlock(&queue->lock);

    return retval;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    // TODO Not yet implemented
    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    // TODO Not yet implemented
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->queue_not_empty);
    pthread_cond_destroy(&queue->queue_not_full);

    return 0;
}
