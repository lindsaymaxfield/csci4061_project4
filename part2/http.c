#include "http.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFSIZE 512

const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    } else if (strcmp(".mp3", file_extension) == 0) {
        return "audio/mpeg";
    }

    return NULL;
}

// Returns file extension as a string
const char *get_file_extension(const char *resource_path) {
    const char *extension =
        strrchr(resource_path, '.');    // get pointer to last occurrance of '.' in the string
    return extension;    // will either return the file extension or NULL if '.' was not found
}

int read_http_request(int fd, char *resource_name) {
    char buffer[BUFSIZE];
    // int num_bytes_read;

    if (read(fd, buffer, BUFSIZE) == -1) {
        perror("read");
        return -1;
    }

    char *token =
        strtok(buffer, " ");    // specify the string to parse for the first call to strtok
    if (token != NULL) {
        token = strtok(NULL, " ");    // call strtok again, this will have resource_name
    }
    if (token == NULL) {
        // Reach this if either of the strtok calss was NULL
        printf("strtok error\n");
        return -1;
    }

    strcpy(resource_name, token);

    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    char *response = malloc(10);
    int capacity = 10;
    strcpy(response, "HTTP/1.0 ");    // http response to build
    char *message = "";               // will hold either "404 Not Found" or "200 OK"
    char *endline = "\r\n";
    int file_exists = 1;

    struct stat stat_buf;
    // inspect file metadata to determine if file exists and get file size
    if (stat(resource_path, &stat_buf) == -1) {
        if (errno == ENOENT) {    // requested file with given path does not exist, don't exit
            message = "404 Not Found";
            file_exists = 0;
        } else {    // other error occurred, exit
            perror("stat");
            free(response);
            return -1;
        }
    } else {    // file exists
        message = "200 OK";
    }

    if (file_exists) {
        int resource = open(resource_path, O_RDONLY,
                            S_IRUSR);    // open file to read, give read permissions to user
        if (resource == -1) {
            perror("open");
            free(response);
            return -1;
        }

        response = realloc(response, strlen(message) + strlen(endline) + capacity);
        if (response == NULL) {
            fprintf(stderr, "realloc\n");
            close(resource);
            free(response);
            return -1;
        }
        capacity = capacity + strlen(message) + strlen(endline);
        strcat(response, message);    // add "200 OK" to response
        strcat(response, endline);    // add endline to response

        // Find content type and file length
        const char *extension = get_file_extension(resource_path);
        const char *mime_type = get_mime_type(extension);
        // off_t file_size = stat_buf.st_size;
        char file_size[12];                                            // idk why I did 12 yet
        snprintf(file_size, 12, "%d", (unsigned) stat_buf.st_size);    // convert st_size to string

        // Add Content-Type line to response header
        response = realloc(
            response, strlen("Content-Type: ") + strlen(mime_type) + strlen(endline) + capacity);
        if (response == NULL) {
            fprintf(stderr, "realloc\n");
            close(resource);
            free(response);
            return -1;
        }
        capacity = capacity + strlen("Content-Type: ") + strlen(mime_type) + strlen(endline);
        strcat(response, "Content-Type: ");
        strcat(response, mime_type);
        strcat(response, endline);

        // Add Content-Length line to response header
        response = realloc(
            response, strlen("Content-Length: ") + strlen(file_size) + strlen(endline) + capacity);
        if (response == NULL) {
            fprintf(stderr, "realloc\n");
            close(resource);
            free(response);
            return -1;
        }
        capacity = capacity + strlen("Content-Length: ") + strlen(file_size) + strlen(endline);
        strcat(response, "Content-Length: ");
        strcat(response, file_size);
        strcat(response, endline);

        // Add another endline for end of header before adding the body
        response = realloc(response, strlen(endline) + capacity);
        if (response == NULL) {
            fprintf(stderr, "realloc\n");
            close(resource);
            free(response);
            return -1;
        }
        capacity = capacity + strlen(endline);
        strcat(response, endline);

        // Read the file in chunks and add to the response in chunks
        int num_bytes_read = 0;
        char buffer[BUFSIZE];
        while ((num_bytes_read = read(resource, buffer, BUFSIZE)) > 0) {
            response = realloc(response, num_bytes_read + capacity);
            if (response == NULL) {
                fprintf(stderr, "realloc\n");
                close(resource);
                free(response);
                return -1;
            }
            capacity = capacity + num_bytes_read;
            strncat(response, buffer, num_bytes_read);
        }
        if (num_bytes_read == -1) {    // read error occurred
            perror("read");
            close(resource);
            free(response);
            return -1;
        }

        // Write the response to the client
        if (write(fd, response, strlen(response)) == -1) {
            perror("write");
            close(resource);
            free(response);
            return -1;
        }

        // Close resource file
        if (close(resource) == -1) {
            perror("close");
            free(response);
            return -1;
        }
    } else {    // file does not exist
        response = realloc(response, strlen(message) + strlen(endline) + capacity);
        if (response == NULL) {
            fprintf(stderr, "realloc\n");
            free(response);
            return -1;
        }
        capacity = capacity + strlen(message) + strlen(endline);
        strcat(response, message);    // add "200 OK" to response
        strcat(response, endline);    // add endline to response

        // Add Content-Length line to response header
        response = realloc(response, strlen("Content-Length: 0\r\n") + capacity);
        if (response == NULL) {
            fprintf(stderr, "realloc\n");
            free(response);
            return -1;
        }
        capacity = capacity + strlen("Content-Length: 0\r\n");
        strcat(response, "Content-Length: 0\r\n");

        // Add another endline for end of header
        response = realloc(response, strlen(endline) + capacity);
        if (response == NULL) {
            fprintf(stderr, "realloc\n");
            free(response);
            return -1;
        }
        capacity = capacity + strlen(endline);
        strcat(response, endline);

        // Write the response to the client
        if (write(fd, response, strlen(response)) == -1) {
            perror("write");
            free(response);
            return -1;
        }
    }

    free(response);

    return 0;
}
