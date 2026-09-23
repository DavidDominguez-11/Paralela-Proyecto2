#include "project2/search.h"

#include "project2/des_crypto.h"

#include <stdlib.h>

bool project2_buffer_contains(
    const unsigned char *buffer,
    size_t buffer_length,
    const unsigned char *needle,
    size_t needle_length
) {
    size_t offset;
    size_t index;

    if (buffer == NULL || needle == NULL || needle_length == 0U ||
        needle_length > buffer_length) {
        return false;
    }

    for (offset = 0; offset <= buffer_length - needle_length; ++offset) {
        for (index = 0; index < needle_length; ++index) {
            if (buffer[offset + index] != needle[index]) {
                break;
            }
        }
        if (index == needle_length) {
            return true;
        }
    }

    return false;
}

Project2Status project2_try_key(
    uint64_t key,
    const unsigned char *ciphertext,
    size_t ciphertext_length,
    const unsigned char *known_phrase,
    size_t known_phrase_length,
    unsigned char *work_buffer,
    size_t work_buffer_capacity,
    bool *matches
) {
    Project2Status status;
    size_t plaintext_length = 0;

    if (known_phrase == NULL || known_phrase_length == 0U || matches == NULL) {
        return PROJECT2_ERR_ARGUMENT;
    }

    *matches = false;
    status = project2_des_decrypt(
        key,
        ciphertext,
        ciphertext_length,
        work_buffer,
        work_buffer_capacity,
        &plaintext_length
    );
    if (status == PROJECT2_ERR_CRYPTO) {
        return PROJECT2_OK;
    }
    if (status != PROJECT2_OK) {
        return status;
    }

    *matches = project2_buffer_contains(
        work_buffer,
        plaintext_length,
        known_phrase,
        known_phrase_length
    );
    return PROJECT2_OK;
}

Project2Status project2_search_range(
    const unsigned char *ciphertext,
    size_t ciphertext_length,
    const unsigned char *known_phrase,
    size_t known_phrase_length,
    uint64_t start_key,
    uint64_t end_key,
    uint64_t stride,
    Project2SearchResult *result
) {
    unsigned char *work_buffer;
    uint64_t key;
    double start_time;

    if (ciphertext == NULL || ciphertext_length == 0U || known_phrase == NULL ||
        known_phrase_length == 0U || result == NULL || stride == 0U ||
        start_key > end_key || end_key > PROJECT2_DES_KEYSPACE) {
        return PROJECT2_ERR_ARGUMENT;
    }

    result->found = false;
    result->key = 0;
    result->attempts = 0;
    result->elapsed_seconds = 0.0;

    work_buffer = malloc(ciphertext_length + 1U);
    if (work_buffer == NULL) {
        return PROJECT2_ERR_MEMORY;
    }

    start_time = project2_monotonic_seconds();
    for (key = start_key; key < end_key; key += stride) {
        bool matches;
        Project2Status status = project2_try_key(
            key,
            ciphertext,
            ciphertext_length,
            known_phrase,
            known_phrase_length,
            work_buffer,
            ciphertext_length + 1U,
            &matches
        );

        ++result->attempts;
        if (status != PROJECT2_OK) {
            free(work_buffer);
            return status;
        }
        if (matches) {
            result->found = true;
            result->key = key;
            break;
        }
        if (UINT64_MAX - key < stride) {
            break;
        }
    }
    result->elapsed_seconds = project2_monotonic_seconds() - start_time;
    free(work_buffer);

    return result->found ? PROJECT2_OK : PROJECT2_ERR_NOT_FOUND;
}

