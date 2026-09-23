#include "project2/des_crypto.h"

#include <openssl/des.h>
#include <stdlib.h>
#include <string.h>

static bool key_is_valid(uint64_t key) {
    return key < PROJECT2_DES_KEYSPACE;
}

static void expand_key(uint64_t key, DES_cblock *block) {
    size_t index;

    for (index = 0; index < PROJECT2_DES_BLOCK_SIZE; ++index) {
        unsigned int shift = (unsigned int)((7U - index) * 7U);
        (*block)[index] = (unsigned char)(((key >> shift) & UINT64_C(0x7f)) << 1U);
    }
    DES_set_odd_parity(block);
}

static void prepare_schedule(uint64_t key, DES_key_schedule *schedule) {
    DES_cblock block;

    expand_key(key, &block);
    DES_set_key_unchecked(&block, schedule);
}

Project2Status project2_des_encrypt(
    uint64_t key,
    const unsigned char *plaintext,
    size_t plaintext_length,
    unsigned char **ciphertext,
    size_t *ciphertext_length
) {
    DES_key_schedule schedule;
    unsigned char *padded;
    unsigned char *encrypted;
    size_t padded_length;
    size_t padding;
    size_t offset;

    if (!key_is_valid(key) || plaintext == NULL || ciphertext == NULL ||
        ciphertext_length == NULL) {
        return PROJECT2_ERR_ARGUMENT;
    }

    if (plaintext_length > SIZE_MAX - PROJECT2_DES_BLOCK_SIZE) {
        return PROJECT2_ERR_ARGUMENT;
    }

    *ciphertext = NULL;
    *ciphertext_length = 0;
    padding = PROJECT2_DES_BLOCK_SIZE -
              (plaintext_length % PROJECT2_DES_BLOCK_SIZE);
    padded_length = plaintext_length + padding;
    padded = malloc(padded_length);
    encrypted = malloc(padded_length);
    if (padded == NULL || encrypted == NULL) {
        free(padded);
        free(encrypted);
        return PROJECT2_ERR_MEMORY;
    }

    memcpy(padded, plaintext, plaintext_length);
    memset(padded + plaintext_length, (int)padding, padding);
    prepare_schedule(key, &schedule);

    for (offset = 0; offset < padded_length; offset += PROJECT2_DES_BLOCK_SIZE) {
        DES_ecb_encrypt(
            (const_DES_cblock *)(padded + offset),
            (DES_cblock *)(encrypted + offset),
            &schedule,
            DES_ENCRYPT
        );
    }

    free(padded);
    *ciphertext = encrypted;
    *ciphertext_length = padded_length;
    return PROJECT2_OK;
}

Project2Status project2_des_decrypt(
    uint64_t key,
    const unsigned char *ciphertext,
    size_t ciphertext_length,
    unsigned char *plaintext,
    size_t plaintext_capacity,
    size_t *plaintext_length
) {
    DES_key_schedule schedule;
    size_t offset;
    size_t index;
    unsigned char padding;

    if (!key_is_valid(key) || ciphertext == NULL || plaintext == NULL ||
        plaintext_length == NULL || ciphertext_length == 0U ||
        ciphertext_length % PROJECT2_DES_BLOCK_SIZE != 0U ||
        plaintext_capacity < ciphertext_length + 1U) {
        return PROJECT2_ERR_ARGUMENT;
    }

    prepare_schedule(key, &schedule);
    for (offset = 0; offset < ciphertext_length; offset += PROJECT2_DES_BLOCK_SIZE) {
        DES_ecb_encrypt(
            (const_DES_cblock *)(ciphertext + offset),
            (DES_cblock *)(plaintext + offset),
            &schedule,
            DES_DECRYPT
        );
    }

    padding = plaintext[ciphertext_length - 1U];
    if (padding == 0U || padding > PROJECT2_DES_BLOCK_SIZE) {
        return PROJECT2_ERR_CRYPTO;
    }

    for (index = 0; index < padding; ++index) {
        if (plaintext[ciphertext_length - 1U - index] != padding) {
            return PROJECT2_ERR_CRYPTO;
        }
    }

    *plaintext_length = ciphertext_length - padding;
    plaintext[*plaintext_length] = '\0';
    return PROJECT2_OK;
}

