#ifndef PROJECT2_COMMON_H
#define PROJECT2_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROJECT2_DES_KEYSPACE (UINT64_C(1) << 56)
#define PROJECT2_DEFAULT_CHECK_INTERVAL UINT64_C(4096)

typedef enum {
    PROJECT2_OK = 0,
    PROJECT2_ERR_ARGUMENT,
    PROJECT2_ERR_IO,
    PROJECT2_ERR_MEMORY,
    PROJECT2_ERR_CRYPTO,
    PROJECT2_ERR_NOT_FOUND
} Project2Status;

const char *project2_status_message(Project2Status status);
bool project2_parse_u64(const char *text, uint64_t *value);
double project2_monotonic_seconds(void);

#endif

