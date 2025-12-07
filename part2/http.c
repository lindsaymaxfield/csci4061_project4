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

/*
 * Get the file extension from a given file path string
 * resource_path: The path of the file to get the extension from
 * Returns file extension as a string or NULL if extension is not found
 */
const char *get_file_extension(const char *resource_path) {
    const char *extension =
        strrchr(resource_path, '.');    // get pointer to last occurrance of '.' in the string
    return extension;    // will either return the file extension or NULL if '.' was not found
}

int read_http_request(int fd, char *resource_name) {
    char buffer[BUFSIZE] = {0};
    int total_bytes_read = 0;

    // continue reading until "\r\n\r\n" found
    // This ensures that the full header is always read even if read doesn't consume the expected
    // number of bytes
    while (1) {
        int bytes_read = read(fd, buffer + total_bytes_read, BUFSIZE - total_bytes_read - 1);

        if (bytes_read < 0) {
            perror("read");
            return -1;
        }
        if (bytes_read == 0)
            break;    // client closed early

        total_bytes_read += bytes_read;
        buffer[total_bytes_read] = '\0';    // Null termination so next call to strstr is safe

        // stop when the "\r\n\r\n" is detected
        if (strstr(buffer, "\r\n\r\n") != NULL) {
            break;
        }
    }

    char *token = strtok(buffer, " ");
    if (token != NULL) {
        token = strtok(NULL, " ");
    }

    if (token == NULL) {
        fprintf(stderr, "strtok error\n");
        return -1;
    }

    strcpy(resource_name, token);

    // Ensure resource_name does not have extra \r or \n character due to unlucky thread scheduling
    int request_length = strlen(resource_name);
    while (request_length > 0 && (resource_name[request_length - 1] == '\n' ||
                                  resource_name[request_length - 1] == '\r')) {
        resource_name[request_length - 1] = '\0';
        request_length--;
    }
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

        // Calculate size of header to write to client
        int capacity =
            snprintf(NULL, 0, "HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %s\r\n\r\n",
                     message, mime_type, file_size) +
            1;
        char header[capacity];

        // Put together header for writing to the client
        snprintf(header, capacity, "HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %s\r\n\r\n",
                 message, mime_type, file_size);

        // Write header to the client
        if (write(fd, header, strlen(header)) == -1) {
            if (errno != ECONNRESET) {
                // If peer resets on shutdown, do not print error message
                perror("write");
            }
            close(resource);
            return -1;
        }

        // Read the file in chunks and write to the client in chunks
        ssize_t num_bytes_read = 0;
        char buffer[BUFSIZE];
        while ((num_bytes_read = read(resource, buffer, BUFSIZE)) > 0) {
            // Write buffer to client
            ssize_t total_written = 0;
            ssize_t amount_to_write = 0;
            while (total_written < num_bytes_read) {
                amount_to_write = num_bytes_read - total_written;
                ssize_t bytes_written = write(fd, buffer + total_written, amount_to_write);

                if (bytes_written < 0) {
                    if (errno != ECONNRESET) {
                        // Peer resetting on shutdown is expected
                        perror("write");
                    }
                    close(resource);
                    return -1;
                }
                total_written += bytes_written;
            }
        }
        if (num_bytes_read == -1) {    // read error occurred
            if (errno != ECONNRESET) {
                // If peer resets on shutdown, do not print error message
                perror("read");
            }
            close(resource);
            return -1;
        }

        // Close resource file
        if (close(resource) == -1) {
            if (errno != ECONNRESET) {
                // If peer resets on shutdown, do not print error message
                perror("close");
            }
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
            if (errno != ECONNRESET) {
                // If peer resets on shutdown, do not print error message
                perror("write");
            }
            return -1;
        }
    }

    return 0;
}
