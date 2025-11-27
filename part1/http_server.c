#define _GNU_SOURCE

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5

int keep_going = 1;

void handle_sigint(int signo) {
    keep_going = 0;
}

int main(int argc, char **argv) {
    // First argument is directory to serve, second is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }

    const char *serve_dir = argv[1];
    const char *port = argv[2];

    // Catch SIGINT so we can clean up properly
    struct sigaction sigact;
    sigact.sa_handler = handle_sigint;
    sigfillset(&sigact.sa_mask);
    sigact.sa_flags = 0;    // No SA_RESTART
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    // Setup TCP Server
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;    // Will be acting as a server

    struct addrinfo *server;

    int ret_val = getaddrinfo(NULL, port, &hints, &server);
    if (ret_val != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret_val));
        return 1;
    }

    int sock_fd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sock_fd == -1) {
        perror("socket");
        freeaddrinfo(server);
        return 1;
    }

    // May need to add code to deal with binding to already taken port...we'll see
    if (bind(sock_fd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(server);
        close(sock_fd);
        return 1;
    }

    if (listen(sock_fd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        freeaddrinfo(server);
        close(sock_fd);
        return 1;
    }

    freeaddrinfo(server);

    while (keep_going != 0) {
        // Wait to receive a connection request from client
        int client_fd = accept(sock_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno != EINTR) { // accept failed
                perror("accept");
                close(sock_fd);
                return 1;
            } else { // need to shut down server since accept() was interrupted by a signal
                break;
            }
        }

        // Get resource name from client
        char *resource_name = "";
        if (read_http_request(client_fd, resource_name) == -1) {
            // Error message will print in read_http_request()
            close(client_fd);
            close(sock_fd);
            return 1;
        }

        // Convert the requested resource name to a proper file path.
        char *resource_path = "";
        strcpy(resource_path, serve_dir); // copies serve_dir to resource_path so that strcat() does not change serve_dir directly
        strcat(resource_path, resource_name); // append resource_name to serve_dir and store in resource_path

        // Call write_http_response() providing the full path to the resource as an argument.
        if (write_http_response(client_fd, resource_path) == -1) {
            // Error message will print in write_http_response()
            close(client_fd);
            close(sock_fd);
            return 1;
        }


        if (close(client_fd) == -1) {
            perror("close");
            close(sock_fd);
            return 1;
        }
    }

    if (close(sock_fd) == -1) {
        perror("close");
        return 1;
    }

    return 0;
}
