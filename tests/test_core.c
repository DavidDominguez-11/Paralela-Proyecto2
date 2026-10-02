#include "project2/des_crypto.h"
#include "project2/file_io.h"
#include "project2/search.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_parse_u64(void) {
    uint64_t value = 0;

    assert(project2_parse_u64("0", &value) && value == 0U);
    assert(project2_parse_u64("42", &value) && value == 42U);
    assert(project2_parse_u64(" 42", &value) && value == 42U);
    assert(project2_parse_u64("18446744073709551615", &value));
    assert(value == UINT64_MAX);
    assert(!project2_parse_u64(NULL, &value));
    assert(!project2_parse_u64("", &value));
    assert(!project2_parse_u64("-1", &value));
    assert(!project2_parse_u64(" -1", &value));
    assert(!project2_parse_u64("42 ", &value));
    assert(!project2_parse_u64("texto", &value));
    assert(!project2_parse_u64("18446744073709551616", &value));
    assert(!project2_parse_u64("42", NULL));
}

static void assert_round_trip(
    uint64_t key,
    const unsigned char *message,
    size_t message_length
) {
    unsigned char *ciphertext = NULL;
    unsigned char *plaintext;
    size_t ciphertext_length = 0;
    size_t plaintext_length = 0;
    Project2Status status;

    status = project2_des_encrypt(
        key,
        message,
        message_length,
        &ciphertext,
        &ciphertext_length
    );
    assert(status == PROJECT2_OK);
    assert(ciphertext_length > 0U);
    assert(ciphertext_length % PROJECT2_DES_BLOCK_SIZE == 0U);

    plaintext = malloc(ciphertext_length + 1U);
    assert(plaintext != NULL);
    status = project2_des_decrypt(
        key,
        ciphertext,
        ciphertext_length,
        plaintext,
        ciphertext_length + 1U,
        &plaintext_length
    );
    assert(status == PROJECT2_OK);
    assert(plaintext_length == message_length);
    assert(memcmp(plaintext, message, message_length) == 0);

    free(ciphertext);
    free(plaintext);
}

static void test_des_limits_and_padding(void) {
    static const unsigned char empty[] = "";
    static const unsigned char block[] = "ABCDEFGH";
    unsigned char *ciphertext = NULL;
    unsigned char plaintext[2U * PROJECT2_DES_BLOCK_SIZE + 1U];
    size_t ciphertext_length = 0;
    size_t plaintext_length = 0;
    Project2Status status;

    assert_round_trip(0U, empty, 0U);
    assert_round_trip(PROJECT2_DES_KEYSPACE - 1U, block, sizeof(block) - 1U);

    status = project2_des_encrypt(
        PROJECT2_DES_KEYSPACE,
        block,
        sizeof(block) - 1U,
        &ciphertext,
        &ciphertext_length
    );
    assert(status == PROJECT2_ERR_ARGUMENT);

    status = project2_des_encrypt(
        42U,
        block,
        sizeof(block) - 1U,
        &ciphertext,
        &ciphertext_length
    );
    assert(status == PROJECT2_OK);
    assert(ciphertext_length == 2U * PROJECT2_DES_BLOCK_SIZE);

    memcpy(
        ciphertext + PROJECT2_DES_BLOCK_SIZE,
        ciphertext,
        PROJECT2_DES_BLOCK_SIZE
    );
    status = project2_des_decrypt(
        42U,
        ciphertext,
        ciphertext_length,
        plaintext,
        sizeof(plaintext),
        &plaintext_length
    );
    assert(status == PROJECT2_ERR_CRYPTO);
    assert(project2_des_decrypt(
        42U,
        ciphertext,
        ciphertext_length - 1U,
        plaintext,
        sizeof(plaintext),
        &plaintext_length
    ) == PROJECT2_ERR_ARGUMENT);

    free(ciphertext);
}

static void test_binary_search(void) {
    static const unsigned char buffer[] = {'A', 0U, 'B', 'C', 0U, 'D'};
    static const unsigned char present[] = {'B', 'C', 0U};
    static const unsigned char absent[] = {'C', 'D'};

    assert(project2_buffer_contains(
        buffer,
        sizeof(buffer),
        present,
        sizeof(present)
    ));
    assert(!project2_buffer_contains(
        buffer,
        sizeof(buffer),
        absent,
        sizeof(absent)
    ));
    assert(!project2_buffer_contains(buffer, sizeof(buffer), present, 0U));
}

static void test_file_io(void) {
    static const char binary_path[] = "build/test_file_io.bin";
    static const char empty_path[] = "build/test_file_io_empty.bin";
    static const unsigned char expected[] = {'A', 0U, 'B', 0xffU, 'C'};
    unsigned char *actual = NULL;
    size_t actual_length = 0;

    assert(project2_write_file(binary_path, expected, sizeof(expected)) == PROJECT2_OK);
    assert(project2_read_file(binary_path, &actual, &actual_length) == PROJECT2_OK);
    assert(actual_length == sizeof(expected));
    assert(memcmp(actual, expected, sizeof(expected)) == 0);
    free(actual);
    actual = NULL;

    assert(project2_write_file(empty_path, NULL, 0U) == PROJECT2_OK);
    assert(project2_read_file(empty_path, &actual, &actual_length) == PROJECT2_OK);
    assert(actual_length == 0U);
    free(actual);

    assert(project2_read_file(
        "build/archivo_que_no_existe",
        &actual,
        &actual_length
    ) == PROJECT2_ERR_IO);
    assert(remove(binary_path) == 0);
    assert(remove(empty_path) == 0);
}

static void test_search(void) {
    static const unsigned char message[] = "Esta es una prueba de proyecto 2";
    static const unsigned char phrase[] = "es una prueba de";
    static const unsigned char absent_phrase[] = "frase que no existe";
    const uint64_t expected_key = 42;
    unsigned char *ciphertext = NULL;
    unsigned char *plaintext = NULL;
    size_t ciphertext_length = 0;
    size_t plaintext_length = 0;
    Project2SearchResult result;
    Project2Status status;
    bool matches = false;

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
        0,
        100,
        2,
        &result
    );
    assert(status == PROJECT2_OK);
    assert(result.key == expected_key);
    assert(result.attempts == expected_key / 2U + 1U);

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
        absent_phrase,
        sizeof(absent_phrase) - 1U,
        0,
        100,
        1,
        &result
    );
    assert(status == PROJECT2_ERR_NOT_FOUND);
    assert(result.attempts == 100U);

    status = project2_search_range(
        ciphertext,
        ciphertext_length,
        phrase,
        sizeof(phrase) - 1U,
        50,
        50,
        1,
        &result
    );
    assert(status == PROJECT2_ERR_NOT_FOUND);
    assert(result.attempts == 0U);

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

    assert(project2_search_range(
        ciphertext,
        ciphertext_length,
        phrase,
        sizeof(phrase) - 1U,
        100,
        99,
        1,
        &result
    ) == PROJECT2_ERR_ARGUMENT);
    assert(project2_search_range(
        ciphertext,
        ciphertext_length,
        phrase,
        sizeof(phrase) - 1U,
        0,
        PROJECT2_DES_KEYSPACE + 1U,
        1,
        &result
    ) == PROJECT2_ERR_ARGUMENT);

    free(ciphertext);
    free(plaintext);
}

int main(void) {
    test_parse_u64();
    test_des_limits_and_padding();
    test_binary_search();
    test_file_io();
    test_search();
    puts("OK: parser, archivos, DES, padding y busqueda secuencial.");
    return EXIT_SUCCESS;
}
