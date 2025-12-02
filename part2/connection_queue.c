#include "connection_queue.h"

#include <stdio.h>
#include <string.h>

int connection_queue_init(connection_queue_t *queue) {
    // TODO: Check if malloc needed here
    queue->read_idx = -1;
    queue->write_idx = 0;
    queue->length = 0;
    queue->shutdown = 0;

    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->queue_full, NULL);
    pthread_cond_init(&queue->queue_empty, NULL);

    return 0;
}

int connection_queue_enqueue(connection_queue_t *queue, int connection_fd) {
    // TODO Not yet implemented
    return 0;
}

int connection_queue_dequeue(connection_queue_t *queue) {
    // TODO Not yet implemented
    return 0;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    // TODO Not yet implemented
    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    // TODO Not yet implemented
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->queue_empty);
    pthread_cond_destroy(&queue->queue_full);

    return 0;
}
