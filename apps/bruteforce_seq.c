#include "project2/common.h"
#include "project2/des_crypto.h"
#include "project2/file_io.h"
#include "project2/search.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *input_path;
    const char *phrase;
    uint64_t start_key;
    uint64_t end_key;
} Options;

static void print_usage(const char *program) {
    fprintf(
        stderr,
        "Uso: %s --input CIFRADO --phrase FRASE [--start-key N] --max-key N\n"
        "El rango de busqueda es semiabierto: [start-key, max-key).\n",
        program
    );
}

static bool parse_arguments(int argc, char **argv, Options *options) {
    int index;
    bool has_max_key = false;

    options->input_path = NULL;
    options->phrase = NULL;
    options->start_key = 0;
    options->end_key = 0;

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--input") == 0 && index + 1 < argc) {
            options->input_path = argv[++index];
        } else if (strcmp(argv[index], "--phrase") == 0 && index + 1 < argc) {
            options->phrase = argv[++index];
        } else if (strcmp(argv[index], "--start-key") == 0 && index + 1 < argc) {
            if (!project2_parse_u64(argv[++index], &options->start_key)) {
                return false;
            }
        } else if (strcmp(argv[index], "--max-key") == 0 && index + 1 < argc) {
            has_max_key = project2_parse_u64(argv[++index], &options->end_key);
            if (!has_max_key) {
                return false;
            }
        } else {
            return false;
        }
    }

    return options->input_path != NULL && options->phrase != NULL &&
           options->phrase[0] != '\0' && has_max_key &&
           options->start_key < options->end_key &&
           options->end_key <= PROJECT2_DES_KEYSPACE;
}

int main(int argc, char **argv) {
    Options options;
    unsigned char *ciphertext = NULL;
    unsigned char *plaintext = NULL;
    size_t ciphertext_length = 0;
    size_t plaintext_length = 0;
    Project2SearchResult result;
    Project2Status status;

    if (!parse_arguments(argc, argv, &options)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    status = project2_read_file(
        options.input_path,
        &ciphertext,
        &ciphertext_length
    );
    if (status != PROJECT2_OK) {
        fprintf(stderr, "No se pudo leer '%s': %s.\n", options.input_path,
                project2_status_message(status));
        return EXIT_FAILURE;
    }

    status = project2_search_range(
        ciphertext,
        ciphertext_length,
        (const unsigned char *)options.phrase,
        strlen(options.phrase),
        options.start_key,
        options.end_key,
        1,
        &result
    );

    printf(
        "Modo: secuencial\nArchivo: %s\nFrase conocida: %s\n"
        "Rango: [%" PRIu64 ", %" PRIu64 ")\nIntentos: %" PRIu64
        "\nTiempo: %.6f s\n",
        options.input_path,
        options.phrase,
        options.start_key,
        options.end_key,
        result.attempts,
        result.elapsed_seconds
    );

    if (status == PROJECT2_ERR_NOT_FOUND) {
        printf("Resultado: llave no encontrada\n");
        free(ciphertext);
        return EXIT_FAILURE;
    }
    if (status != PROJECT2_OK) {
        fprintf(stderr, "La busqueda fallo: %s.\n",
                project2_status_message(status));
        free(ciphertext);
        return EXIT_FAILURE;
    }

    plaintext = malloc(ciphertext_length + 1U);
    if (plaintext == NULL) {
        free(ciphertext);
        return EXIT_FAILURE;
    }

    status = project2_des_decrypt(
        result.key,
        ciphertext,
        ciphertext_length,
        plaintext,
        ciphertext_length + 1U,
        &plaintext_length
    );
    if (status != PROJECT2_OK) {
        fprintf(stderr, "No se pudo recuperar el texto: %s.\n",
                project2_status_message(status));
        free(ciphertext);
        free(plaintext);
        return EXIT_FAILURE;
    }

    printf("Resultado: llave encontrada\nLlave: %" PRIu64 "\nTexto: ", result.key);
    fwrite(plaintext, 1, plaintext_length, stdout);
    putchar('\n');

    free(ciphertext);
    free(plaintext);
    return EXIT_SUCCESS;
}

