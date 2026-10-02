#include "project2/des_crypto.h"
#include "project2/search.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    static const unsigned char message[] = "Esta es una prueba de proyecto 2";
    static const unsigned char phrase[] = "es una prueba de";
    const uint64_t expected_key = 42;
    unsigned char *ciphertext = NULL;
    unsigned char *plaintext = NULL;
    size_t ciphertext_length = 0;
    size_t plaintext_length = 0;
    Project2SearchResult result;
    Project2Status status;
    bool matches = false;
    uint64_t parsed_value = 0;

    assert(project2_parse_u64("42", &parsed_value));
    assert(parsed_value == 42U);
    assert(!project2_parse_u64("-1", &parsed_value));
    assert(!project2_parse_u64(" -1", &parsed_value));

    status = project2_des_encrypt(
        expected_key,
        message,
        sizeof(message) - 1U,
        &ciphertext,
        &ciphertext_length
    );
    assert(status == PROJECT2_OK);
    assert(ciphertext_length % PROJECT2_DES_BLOCK_SIZE == 0U);

    plaintext = malloc(ciphertext_length + 1U);
    assert(plaintext != NULL);
    status = project2_des_decrypt(
        expected_key,
        ciphertext,
        ciphertext_length,
        plaintext,
        ciphertext_length + 1U,
        &plaintext_length
    );
    assert(status == PROJECT2_OK);
    assert(plaintext_length == sizeof(message) - 1U);
    assert(memcmp(plaintext, message, plaintext_length) == 0);

    status = project2_try_key(
        expected_key,
        ciphertext,
        ciphertext_length,
        phrase,
        sizeof(phrase) - 1U,
        plaintext,
        ciphertext_length + 1U,
        &matches
    );
    assert(status == PROJECT2_OK && matches);

    status = project2_search_range(
        ciphertext,
        ciphertext_length,
        phrase,
        sizeof(phrase) - 1U,
        0,
        100,
        1,
        &result
    );
    assert(status == PROJECT2_OK);
    assert(result.found);
    assert(result.key == expected_key);
    assert(result.attempts == expected_key + 1U);

    status = project2_search_range(
        ciphertext,
        ciphertext_length,
        phrase,
        sizeof(phrase) - 1U,
        expected_key + 1U,
        100,
        1,
        &result
    );
    assert(status == PROJECT2_ERR_NOT_FOUND);
    assert(!result.found);

    status = project2_search_range(
        ciphertext,
        ciphertext_length,
        phrase,
        sizeof(phrase) - 1U,
        0,
        100,
        0,
        &result
    );
    assert(status == PROJECT2_ERR_ARGUMENT);

    free(ciphertext);
    free(plaintext);
    puts("OK: cifrado, descifrado, validacion y busqueda secuencial.");
    return EXIT_SUCCESS;
}
