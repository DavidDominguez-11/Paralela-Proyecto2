#ifndef PROJECT2_FILE_IO_H
#define PROJECT2_FILE_IO_H

#include "project2/common.h"

Project2Status project2_read_file(
    const char *path,
    unsigned char **data,
    size_t *length
);

Project2Status project2_write_file(
    const char *path,
    const unsigned char *data,
    size_t length
);

#endif

