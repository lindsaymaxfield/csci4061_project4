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

    if (pthread_mutex_init(&queue->lock, NULL)) {
        perror("pthread_mutex_init");
        return -1;
    }
    if (pthread_cond_init(&queue->queue_not_full, NULL)) {
        perror("pthread_cond_init");
        pthread_mutex_destroy(&queue->lock);
        return -1;
    }
    if (pthread_cond_init(&queue->queue_not_empty, NULL)) {
        perror("ptread_cond_init");
        pthread_mutex_destroy(&queue->lock);
        pthread_cond_destroy(&queue->queue_not_full);
        return -1;
    }

    return 0;
}

int connection_queue_enqueue(connection_queue_t *queue, int connection_fd) {
    // TODO Implement Shutdown
    if (pthread_mutex_lock(&queue->lock)) {
        perror("pthread_mutex_lock");
        return -1;
    }

    while (queue->length == CAPACITY && !queue->shutdown) {
        if (pthread_cond_wait(&queue->queue_not_full, &queue->lock)) {
            perror("pthread_cond_wait");
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }

    if (queue->shutdown) {
        if (pthread_mutex_unlock(&queue->lock)) {
            perror("pthread_mutex_unlock");
        }
        return -1;
    }

    queue->client_fds[queue->write_idx] = connection_fd;
    queue->write_idx = (queue->write_idx + 1) % CAPACITY;
    queue->length++;

    if (pthread_cond_signal(&queue->queue_not_empty)) {
        perror("pthread_cond_signal");
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    if (pthread_mutex_unlock(&queue->lock)) {
        perror("pthread_mutex_unlock");
        return -1;
    }

    return 0;
}

int connection_queue_dequeue(connection_queue_t *queue) {
    // TODO: Implement shutdown
    if (pthread_mutex_lock(&queue->lock)) {
        perror("pthread_mutex_lock");
        return -1;
    }

    while (queue->length == 0 && !queue->shutdown) {
        if (pthread_cond_wait(&queue->queue_not_empty, &queue->lock)) {
            perror("pthread_cond_wait");
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }

    if (queue->shutdown) {
        if (pthread_mutex_unlock(&queue->lock)) {
            perror("pthread_mutex_unlock");
        }
        return -1;
    }

    int retval = queue->client_fds[queue->read_idx];
    queue->length--;
    queue->read_idx = (queue->read_idx + 1) % CAPACITY;

    if (pthread_cond_signal(&queue->queue_not_full)) {
        perror("pthread_cond_signal");
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    if (pthread_mutex_unlock(&queue->lock)) {
        perror("pthread_mutex_unlock");
        return -1;
    }

    return retval;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    // TODO Not yet implemented
    if (pthread_mutex_lock(&queue->lock)) {
        perror("pthread_mutex_lock");
        return -1;
    }
    queue->shutdown = 1;
    // send both signals to wake up all blocked threads
    if (pthread_cond_broadcast(&queue->queue_not_full)) {
        perror("pthread_cond_broadcast");
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    if (pthread_cond_broadcast(&queue->queue_not_empty)) {
        perror("pthread_cond_broadcast");
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    if (pthread_mutex_unlock(&queue->lock)) {
        perror("pthread_mutex_unlock");
        return -1;
    }

    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    // TODO Not yet implemented
    if (pthread_mutex_destroy(&queue->lock)) {
        perror("pthread_mutex_destroy");
        pthread_cond_destroy(&queue->queue_not_empty);
        pthread_cond_destroy(&queue->queue_not_full);
        return -1;
    }
    if (pthread_cond_destroy(&queue->queue_not_empty)) {
        perror("pthread_cond_destroy");
        pthread_cond_destroy(&queue->queue_not_full);
        return -1;
    }
    if (pthread_cond_destroy(&queue->queue_not_full)) {
        perror("pthread_mutex_destroy");
        return -1;
    }

    return 0;
}
