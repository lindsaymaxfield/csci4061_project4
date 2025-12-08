#include "connection_queue.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief Intializes a thread safe queue for connection file descriptors
 *
 * @details Initializes member variables, intializes one mutex, and two condition variables
 *
 * @param queue A pointer to the desired connection_queue_t to be initialized
 * @return 0 on success, -1 on failure
 */
int connection_queue_init(connection_queue_t *queue) {
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->length = 0;
    queue->shutdown = 0;

    for (int i = 0; i < CAPACITY; ++i)
        queue->client_fds[i] = -1;

    int error_code = 0;
    if ((error_code = pthread_mutex_init(&queue->lock, NULL))) {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(error_code));
        return -1;
    }

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

/**
 * @brief adds a file descriptor to the queue, fails is queue is shutdown
 *
 * @details acquires lock, waits for the queue to not be full (w/ cond. var.), adds fd to queue,
 * signals that queue is not empty, then releases the lock
 *
 *
 * @param queue pointer to the queue which the fd is added to
 * @param fd client file descriptor to add (taken from accept())
 *
 * @return 0 on success, -1 on failure
 */
int connection_queue_enqueue(connection_queue_t *queue, int connection_fd) {
    int error_code = 0;
    error_code = pthread_mutex_lock(&queue->lock);
    if (error_code) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(error_code));
        // Clean up handled by caller in http_server
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

/**
 * @brief Removes a file descriptor from the queue, dequeuing can continue on shutdown
 *
 * @details Acquires lock, waits on cond. var. if the queue is empty, removes the fd from the queue,
 * signals that the queue is not full, then releases the lock
 *
 * @param queue - Queue to take the fd from
 * @return 0 on success, -1 on failure
 */
int connection_queue_dequeue(connection_queue_t *queue) {
    int error_code = 0;
    error_code = pthread_mutex_lock(&queue->lock);
    if (error_code) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(error_code));
        // Clean up handled by caller in htpp_server
        return -1;
    }

    while (queue->length == 0 && !queue->shutdown) {
        error_code = pthread_cond_wait(&queue->queue_not_empty, &queue->lock);
        if (error_code) {
            fprintf(stderr, "pthread_cond_wait: %s\n", strerror(error_code));
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }

    // Only exit if no work to do
    if (queue->length == 0 && queue->shutdown) {
        error_code = pthread_mutex_unlock(&queue->lock);
        if (error_code) {
            fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(error_code));
            return -1;
        }
        return -1;
    }

    int fd = queue->client_fds[queue->read_idx];
    queue->read_idx = (queue->read_idx + 1) % CAPACITY;
    queue->length--;

    error_code = pthread_cond_signal(&queue->queue_not_full);
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
    return fd;
}

/**
 * @brief Notifies threads that queue is shutdown for safe cleanup
 *
 * @details Acquires lock, sets shutdown flag, broadcasts all condition variables, then releases the
 * lock
 *
 * @param queue Queue to be shutdown
 * @return 0 on success, -1 on error
 */
int connection_queue_shutdown(connection_queue_t *queue) {
    int error_code = 0;
    error_code = pthread_mutex_lock(&queue->lock);
    if (error_code) {
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(error_code));
        // Clean up handled by caller in http_server
        return -1;
    }
    queue->shutdown = 1;
    // send both signals to wake up all blocked threads
    error_code = pthread_cond_broadcast(&queue->queue_not_full);
    if (error_code) {
        fprintf(stderr, "pthread_cond_broadcast: %s\n", strerror(error_code));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    error_code = pthread_cond_broadcast(&queue->queue_not_empty);
    if (error_code) {
        fprintf(stderr, "pthread_cond_broadcast: %s\n", strerror(error_code));
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

/**
 * @brief frees the queue's lock and condition variables
 *
 * @param queue Queue to be freed
 * @return 0 on success, -1 on failure
 */
int connection_queue_free(connection_queue_t *queue) {
    int error_code = 0;
    error_code = pthread_mutex_destroy(&queue->lock);
    if (error_code) {
        fprintf(stderr, "pthread_mutex_destroy: %s\n", strerror(error_code));
        pthread_cond_destroy(&queue->queue_not_empty);
        pthread_cond_destroy(&queue->queue_not_full);
        return -1;
    }
    error_code = pthread_cond_destroy(&queue->queue_not_empty);
    if (error_code) {
        fprintf(stderr, "pthread_cond_destroy: %s\n", strerror(error_code));
        pthread_cond_destroy(&queue->queue_not_full);
        return -1;
    }
    error_code = pthread_cond_destroy(&queue->queue_not_full);
    if (error_code) {
        fprintf(stderr, "pthread_cond_destroy: %s\n", strerror(error_code));
        return -1;
    }

    return 0;
}
