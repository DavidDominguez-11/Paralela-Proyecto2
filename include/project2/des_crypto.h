#ifndef PROJECT2_DES_CRYPTO_H
#define PROJECT2_DES_CRYPTO_H

#include "project2/common.h"

#define PROJECT2_DES_BLOCK_SIZE 8U

Project2Status project2_des_encrypt(
    uint64_t key,
    const unsigned char *plaintext,
    size_t plaintext_length,
    unsigned char **ciphertext,
    size_t *ciphertext_length
);

Project2Status project2_des_decrypt(
    uint64_t key,
    const unsigned char *ciphertext,
    size_t ciphertext_length,
    unsigned char *plaintext,
    size_t plaintext_capacity,
    size_t *plaintext_length
);

#endif

