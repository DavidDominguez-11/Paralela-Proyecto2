#include "project2/file_io.h"

#include <stdio.h>
#include <stdlib.h>

Project2Status project2_read_file(
    const char *path,
    unsigned char **data,
    size_t *length
) {
    FILE *file;
    long file_length;
    unsigned char *buffer;
    size_t bytes_read;

    if (path == NULL || data == NULL || length == NULL) {
        return PROJECT2_ERR_ARGUMENT;
    }

    *data = NULL;
    *length = 0;
    file = fopen(path, "rb");
    if (file == NULL) {
        return PROJECT2_ERR_IO;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return PROJECT2_ERR_IO;
    }

    file_length = ftell(file);
    if (file_length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return PROJECT2_ERR_IO;
    }

    buffer = malloc((size_t)file_length + 1U);
    if (buffer == NULL) {
        fclose(file);
        return PROJECT2_ERR_MEMORY;
    }

    bytes_read = fread(buffer, 1, (size_t)file_length, file);
    if (bytes_read != (size_t)file_length || fclose(file) != 0) {
        free(buffer);
        return PROJECT2_ERR_IO;
    }

    buffer[bytes_read] = '\0';
    *data = buffer;
    *length = bytes_read;
    return PROJECT2_OK;
}

Project2Status project2_write_file(
    const char *path,
    const unsigned char *data,
    size_t length
) {
    FILE *file;
    size_t bytes_written;

    if (path == NULL || (data == NULL && length != 0U)) {
        return PROJECT2_ERR_ARGUMENT;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return PROJECT2_ERR_IO;
    }

    bytes_written = fwrite(data, 1, length, file);
    if (bytes_written != length || fclose(file) != 0) {
        return PROJECT2_ERR_IO;
    }

    return PROJECT2_OK;
}

