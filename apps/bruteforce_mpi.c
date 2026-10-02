#include "project2/common.h"
#include "project2/des_crypto.h"
#include "project2/file_io.h"
#include "project2/search.h"

#include <inttypes.h>
#include <limits.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PROJECT2_MPI_CYCLIC
#define PROJECT2_MPI_MODE "MPI ciclico"
#else
#define PROJECT2_MPI_MODE "MPI naive"
#endif

typedef struct {
    const char *input_path;
    const char *phrase;
    uint64_t max_key;
    uint64_t check_interval;
} Options;

static void print_usage(const char *program) {
    fprintf(
        stderr,
        "Uso: mpirun -np N %s --input CIFRADO --phrase FRASE --max-key N "
        "[--check-interval N]\n",
        program
    );
}

static bool parse_arguments(int argc, char **argv, Options *options) {
    int index;
    bool has_max_key = false;

    options->input_path = NULL;
    options->phrase = NULL;
    options->max_key = 0;
    options->check_interval = PROJECT2_DEFAULT_CHECK_INTERVAL;

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--input") == 0 && index + 1 < argc) {
            options->input_path = argv[++index];
        } else if (strcmp(argv[index], "--phrase") == 0 && index + 1 < argc) {
            options->phrase = argv[++index];
        } else if (strcmp(argv[index], "--max-key") == 0 && index + 1 < argc) {
            has_max_key = project2_parse_u64(argv[++index], &options->max_key);
            if (!has_max_key) {
                return false;
            }
        } else if (strcmp(argv[index], "--check-interval") == 0 &&
                   index + 1 < argc) {
            if (!project2_parse_u64(argv[++index], &options->check_interval)) {
                return false;
            }
        } else {
            return false;
        }
    }

    return options->input_path != NULL && options->phrase != NULL &&
           options->phrase[0] != '\0' && has_max_key &&
           options->max_key > 0U && options->max_key <= PROJECT2_DES_KEYSPACE &&
           options->check_interval > 0U;
}

#ifndef PROJECT2_MPI_CYCLIC
static void partition_range(
    uint64_t total,
    int rank,
    int process_count,
    uint64_t *start,
    uint64_t *end
) {
    uint64_t count = (uint64_t)process_count;
    uint64_t rank_u64 = (uint64_t)rank;
    uint64_t quotient = total / count;
    uint64_t remainder = total % count;
    uint64_t prefix_extra = rank_u64 < remainder ? rank_u64 : remainder;
    uint64_t local_count = quotient + (rank_u64 < remainder ? 1U : 0U);

    *start = quotient * rank_u64 + prefix_extra;
    *end = *start + local_count;
}
#endif

int main(int argc, char **argv) {
    int rank;
    int process_count;
    int valid = 1;
    Options options;
    unsigned char *ciphertext = NULL;
    unsigned char *phrase = NULL;
    unsigned char *work_buffer = NULL;
    uint64_t ciphertext_length = 0;
    uint64_t phrase_length = 0;
    uint64_t config[2] = {0, 0};
    uint64_t start_key = 0;
    uint64_t end_key = 0;
    uint64_t key_stride = 1;
    uint64_t next_key;
    uint64_t local_attempts = 0;
    uint64_t total_attempts = 0;
    uint64_t global_key = UINT64_MAX;
    double start_time;
    double local_elapsed;
    double elapsed = 0.0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &process_count);

    if (rank == 0) {
        size_t file_length = 0;
        Project2Status status;

        valid = parse_arguments(argc, argv, &options) ? 1 : 0;
        if (valid) {
            status = project2_read_file(
                options.input_path,
                &ciphertext,
                &file_length
            );
            valid = status == PROJECT2_OK && file_length <= INT_MAX &&
                    strlen(options.phrase) <= INT_MAX;
            if (!valid) {
                fprintf(stderr, "No se pudo preparar la entrada MPI.\n");
            } else {
                ciphertext_length = (uint64_t)file_length;
                phrase_length = (uint64_t)strlen(options.phrase);
                phrase = (unsigned char *)options.phrase;
                config[0] = options.max_key;
                config[1] = options.check_interval;
            }
        }
        if (!valid) {
            print_usage(argv[0]);
        }
    }

    MPI_Bcast(&valid, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!valid) {
        free(ciphertext);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    MPI_Bcast(config, 2, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    MPI_Bcast(&ciphertext_length, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    MPI_Bcast(&phrase_length, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);

    if (rank != 0) {
        ciphertext = malloc((size_t)ciphertext_length);
        phrase = malloc((size_t)phrase_length + 1U);
        valid = ciphertext != NULL && phrase != NULL;
    }
    MPI_Allreduce(MPI_IN_PLACE, &valid, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (!valid) {
        if (rank == 0) {
            fprintf(stderr, "No fue posible reservar memoria en todos los procesos.\n");
        }
        free(ciphertext);
        if (rank != 0) {
            free(phrase);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    MPI_Bcast(ciphertext, (int)ciphertext_length, MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Bcast(phrase, (int)phrase_length, MPI_BYTE, 0, MPI_COMM_WORLD);
    if (rank != 0) {
        phrase[phrase_length] = '\0';
    }

    work_buffer = malloc((size_t)ciphertext_length + 1U);
    valid = work_buffer != NULL;
    MPI_Allreduce(MPI_IN_PLACE, &valid, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (!valid) {
        if (rank == 0) {
            fprintf(stderr, "No fue posible reservar el buffer de busqueda.\n");
        }
        free(ciphertext);
        if (rank != 0) {
            free(phrase);
        }
        free(work_buffer);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

#ifdef PROJECT2_MPI_CYCLIC
    start_key = (uint64_t)rank;
    end_key = config[0];
    key_stride = (uint64_t)process_count;
#else
    partition_range(config[0], rank, process_count, &start_key, &end_key);
#endif
    next_key = start_key;

    MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    while (global_key == UINT64_MAX) {
        uint64_t local_key = UINT64_MAX;
        uint64_t processed = 0;
        int local_error = 0;
        int global_error = 0;
        int local_active;
        int active_processes = 0;

        while (processed < config[1] && next_key < end_key) {
            bool matches = false;
            Project2Status status = project2_try_key(
                next_key,
                ciphertext,
                (size_t)ciphertext_length,
                phrase,
                (size_t)phrase_length,
                work_buffer,
                (size_t)ciphertext_length + 1U,
                &matches
            );

            ++local_attempts;
            ++processed;
            if (status != PROJECT2_OK) {
                local_error = 1;
                break;
            }
            if (matches) {
                local_key = next_key;
                break;
            }
            next_key += key_stride;
        }

        MPI_Allreduce(
            &local_error,
            &global_error,
            1,
            MPI_INT,
            MPI_MAX,
            MPI_COMM_WORLD
        );
        if (global_error) {
            valid = 0;
            break;
        }

        MPI_Allreduce(
            &local_key,
            &global_key,
            1,
            MPI_UINT64_T,
            MPI_MIN,
            MPI_COMM_WORLD
        );
        if (global_key != UINT64_MAX) {
            break;
        }

        local_active = next_key < end_key ? 1 : 0;
        MPI_Allreduce(
            &local_active,
            &active_processes,
            1,
            MPI_INT,
            MPI_SUM,
            MPI_COMM_WORLD
        );
        if (active_processes == 0) {
            break;
        }
    }

    local_elapsed = MPI_Wtime() - start_time;
    MPI_Reduce(
        &local_elapsed,
        &elapsed,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );
    MPI_Reduce(
        &local_attempts,
        &total_attempts,
        1,
        MPI_UINT64_T,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );

    if (rank == 0) {
        printf(
            "Modo: %s\nProcesos: %d\nArchivo: %s\n"
            "Frase conocida: %s\nRango global: [0, %" PRIu64 ")\n"
            "Intervalo de sincronizacion: %" PRIu64
            "\nIntentos totales: %" PRIu64 "\nTiempo: %.9f s\n",
            PROJECT2_MPI_MODE,
            process_count,
            options.input_path,
            options.phrase,
            config[0],
            config[1],
            total_attempts,
            elapsed
        );

        if (!valid) {
            printf("Resultado: error durante la busqueda\n");
        } else if (global_key == UINT64_MAX) {
            printf("Resultado: llave no encontrada\n");
        } else {
            unsigned char *plaintext = malloc((size_t)ciphertext_length + 1U);
            size_t plaintext_length = 0;
            Project2Status status = plaintext == NULL
                ? PROJECT2_ERR_MEMORY
                : project2_des_decrypt(
                      global_key,
                      ciphertext,
                      (size_t)ciphertext_length,
                      plaintext,
                      (size_t)ciphertext_length + 1U,
                      &plaintext_length
                  );

            if (status == PROJECT2_OK) {
                printf("Resultado: llave encontrada\nLlave: %" PRIu64
                       "\nTexto: ", global_key);
                fwrite(plaintext, 1, plaintext_length, stdout);
                putchar('\n');
            } else {
                printf("Resultado: no se pudo recuperar el texto\n");
                valid = 0;
            }
            free(plaintext);
        }
    }

    free(ciphertext);
    if (rank != 0) {
        free(phrase);
    }
    free(work_buffer);
    MPI_Finalize();
    return valid && global_key != UINT64_MAX ? EXIT_SUCCESS : EXIT_FAILURE;
}
