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
        fprintf(stderr, "strtok\n");
        return -1;
    }

    strcpy(resource_name, token);

    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    char *message = "";    // will hold either "404 Not Found" or "200 OK"
    int file_exists = 1;

    struct stat stat_buf;
    // inspect file metadata to determine if file exists and get file size
    if (stat(resource_path, &stat_buf) == -1) {
        if (errno == ENOENT) {    // requested file with given path does not exist, don't exit
            message = "404 Not Found";
            file_exists = 0;
        } else {    // other error occurred, exit
            perror("stat");
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
            return -1;
        }

        // Find content type
        const char *extension = get_file_extension(resource_path);
        const char *mime_type = get_mime_type(extension);

        // Find file length
        char file_size[12];
        snprintf(file_size, 12, "%d", (unsigned) stat_buf.st_size);    // convert st_size to string
        printf("File size is %s bytes\n", file_size);

        // Calculate size of header to write to client
        int capacity = strlen("HTTP/1.0 \r\nContent-Type: \r\nContent-Length: \r\n\r\n") +
                       strlen(message) + strlen(mime_type) + strlen(file_size) + 1;
        char header[capacity];

        // Put together header for writing to the client
        snprintf(header, capacity, "HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %s\r\n\r\n",
                 message, mime_type, file_size);

        // Write header to the client
        if (write(fd, header, strlen(header)) == -1) {
            perror("write");
            close(resource);
            return -1;
        }

        // Read the file in chunks and write to the client in chunks
        int num_bytes_read = 0;
        char buffer[BUFSIZE];
        while ((num_bytes_read = read(resource, buffer, BUFSIZE)) > 0) {
            // Write buffer to client
            if (write(fd, buffer, num_bytes_read) == -1) {
                perror("write");
                close(resource);
                return -1;
            }
        }
        if (num_bytes_read == -1) {    // read error occurred
            perror("read");
            close(resource);
            return -1;
        }

        // Close resource file
        if (close(resource) == -1) {
            perror("close");
            return -1;
        }
    } else {    // file does not exist
        // Calculate size of header to write to client
        int capacity = strlen("HTTP/1.0 \r\nContent-Length: 0\r\n\r\n") + strlen(message);
        char header[capacity];

        // Put together header for writing to the client
        snprintf(header, capacity, "HTTP/1.0 %s\r\nContent-Length: 0\r\n\r\n", message);

        // Write the response to the client
        if (write(fd, header, strlen(header)) == -1) {
            perror("write");
            return -1;
        }
    }

    return 0;
}
