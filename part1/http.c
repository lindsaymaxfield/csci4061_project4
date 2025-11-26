#include "http.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
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

int read_http_request(int fd, char *resource_name) {
    // TODO Not yet implemented
    char *buffer[BUFSIZE];
    int num_bytes_read;

    if (read(fd, buffer, BUFSIZE) == -1) {
        perror("read");
        return -1;
    }

    char *token = strtok(buffer, " "); // specify the string to parse for the first call to strtok
    token = strtok(NULL, " "); // call strtok again, this will have resource_name

    // may need to error check to make sure token isn't NULL here?

    strcpy(resource_name, token);

    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    // TODO Not yet implemented
    return 0;
}
