#ifndef PROJECT2_SEARCH_H
#define PROJECT2_SEARCH_H

#include "project2/common.h"

typedef struct {
    bool found;
    uint64_t key;
    uint64_t attempts;
    double elapsed_seconds;
} Project2SearchResult;

bool project2_buffer_contains(
    const unsigned char *buffer,
    size_t buffer_length,
    const unsigned char *needle,
    size_t needle_length
);

Project2Status project2_try_key(
    uint64_t key,
    const unsigned char *ciphertext,
    size_t ciphertext_length,
    const unsigned char *known_phrase,
    size_t known_phrase_length,
    unsigned char *work_buffer,
    size_t work_buffer_capacity,
    bool *matches
);

Project2Status project2_search_range(
    const unsigned char *ciphertext,
    size_t ciphertext_length,
    const unsigned char *known_phrase,
    size_t known_phrase_length,
    uint64_t start_key,
    uint64_t end_key,
    uint64_t stride,
    Project2SearchResult *result
);

#endif

