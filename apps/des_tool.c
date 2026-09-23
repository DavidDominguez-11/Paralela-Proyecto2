#include "project2/common.h"
#include "project2/des_crypto.h"
#include "project2/file_io.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program) {
    fprintf(
        stderr,
        "Uso:\n"
        "  %s encrypt --input ARCHIVO --output ARCHIVO --key LLAVE\n"
        "  %s decrypt --input ARCHIVO --output ARCHIVO --key LLAVE\n",
        program,
        program
    );
}

static bool parse_arguments(
    int argc,
    char **argv,
    const char **operation,
    const char **input_path,
    const char **output_path,
    uint64_t *key
) {
    int index;
    bool has_key = false;

    if (argc < 2) {
        return false;
    }

    *operation = argv[1];
    *input_path = NULL;
    *output_path = NULL;

    for (index = 2; index < argc; ++index) {
        if (strcmp(argv[index], "--input") == 0 && index + 1 < argc) {
            *input_path = argv[++index];
        } else if (strcmp(argv[index], "--output") == 0 && index + 1 < argc) {
            *output_path = argv[++index];
        } else if (strcmp(argv[index], "--key") == 0 && index + 1 < argc) {
            has_key = project2_parse_u64(argv[++index], key);
            if (!has_key) {
                return false;
            }
        } else {
            return false;
        }
    }

    return (strcmp(*operation, "encrypt") == 0 ||
            strcmp(*operation, "decrypt") == 0) &&
           *input_path != NULL && *output_path != NULL && has_key &&
           *key < PROJECT2_DES_KEYSPACE;
}

int main(int argc, char **argv) {
    const char *operation;
    const char *input_path;
    const char *output_path;
    uint64_t key = 0;
    unsigned char *input = NULL;
    unsigned char *output = NULL;
    size_t input_length = 0;
    size_t output_length = 0;
    Project2Status status;

    if (!parse_arguments(
            argc,
            argv,
            &operation,
            &input_path,
            &output_path,
            &key
        )) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    status = project2_read_file(input_path, &input, &input_length);
    if (status != PROJECT2_OK) {
        fprintf(stderr, "No se pudo leer '%s': %s.\n", input_path,
                project2_status_message(status));
        return EXIT_FAILURE;
    }

    if (strcmp(operation, "encrypt") == 0) {
        status = project2_des_encrypt(
            key,
            input,
            input_length,
            &output,
            &output_length
        );
    } else {
        if (input_length == 0U || input_length > SIZE_MAX - 1U) {
            status = PROJECT2_ERR_ARGUMENT;
        } else {
            output = malloc(input_length + 1U);
            status = output == NULL
                         ? PROJECT2_ERR_MEMORY
                         : project2_des_decrypt(
                               key,
                               input,
                               input_length,
                               output,
                               input_length + 1U,
                               &output_length
                           );
        }
    }

    if (status == PROJECT2_OK) {
        status = project2_write_file(output_path, output, output_length);
    }

    if (status != PROJECT2_OK) {
        fprintf(stderr, "Fallo en %s: %s.\n", operation,
                project2_status_message(status));
        free(input);
        free(output);
        return EXIT_FAILURE;
    }

    printf(
        "Operacion: %s\nLlave: %" PRIu64 "\nEntrada: %s (%zu bytes)\n"
        "Salida: %s (%zu bytes)\n",
        operation,
        key,
        input_path,
        input_length,
        output_path,
        output_length
    );

    free(input);
    free(output);
    return EXIT_SUCCESS;
}

