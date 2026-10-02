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

enum MessageTag {
    TAG_REQUEST = 100,
    TAG_WORK,
    TAG_FOUND,
    TAG_STOP,
    TAG_ERROR
};

typedef struct {
    const char *input_path;
    const char *phrase;
    uint64_t max_key;
    uint64_t chunk_size;
} Options;

static void print_usage(const char *program) {
    fprintf(
        stderr,
        "Uso: mpirun -np N %s --input CIFRADO --phrase FRASE --max-key N "
        "[--chunk-size N]\nSe requieren al menos dos procesos.\n",
        program
    );
}

static bool parse_arguments(int argc, char **argv, Options *options) {
    int index;
    bool has_max_key = false;

    options->input_path = NULL;
    options->phrase = NULL;
    options->max_key = 0;
    options->chunk_size = PROJECT2_DEFAULT_CHUNK_SIZE;

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
        } else if (strcmp(argv[index], "--chunk-size") == 0 &&
                   index + 1 < argc) {
            if (!project2_parse_u64(argv[++index], &options->chunk_size)) {
                return false;
            }
        } else {
            return false;
        }
    }

    return options->input_path != NULL && options->phrase != NULL &&
           options->phrase[0] != '\0' && has_max_key &&
           options->max_key > 0U && options->max_key <= PROJECT2_DES_KEYSPACE &&
           options->chunk_size > 0U;
}

static void send_stop(int worker) {
    const uint64_t stop_message[2] = {0U, 0U};

    MPI_Send(stop_message, 2, MPI_UINT64_T, worker, TAG_STOP, MPI_COMM_WORLD);
}

static void run_master(
    int worker_count,
    uint64_t max_key,
    uint64_t chunk_size,
    uint64_t *global_key,
    int *valid
) {
    uint64_t next_key = 0U;
    int stopped_workers = 0;

    while (stopped_workers < worker_count) {
        MPI_Status mpi_status;

        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);
        if (mpi_status.MPI_TAG == TAG_REQUEST) {
            int request;

            MPI_Recv(
                &request,
                1,
                MPI_INT,
                mpi_status.MPI_SOURCE,
                TAG_REQUEST,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            );
            if (*valid == 0 || *global_key != UINT64_MAX || next_key >= max_key) {
                send_stop(mpi_status.MPI_SOURCE);
                ++stopped_workers;
            } else {
                uint64_t assignment[2];
                uint64_t remaining = max_key - next_key;
                uint64_t assigned = chunk_size < remaining ? chunk_size : remaining;

                assignment[0] = next_key;
                assignment[1] = next_key + assigned;
                next_key = assignment[1];
                MPI_Send(
                    assignment,
                    2,
                    MPI_UINT64_T,
                    mpi_status.MPI_SOURCE,
                    TAG_WORK,
                    MPI_COMM_WORLD
                );
            }
        } else if (mpi_status.MPI_TAG == TAG_FOUND) {
            uint64_t candidate;

            MPI_Recv(
                &candidate,
                1,
                MPI_UINT64_T,
                mpi_status.MPI_SOURCE,
                TAG_FOUND,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            );
            if (candidate < *global_key) {
                *global_key = candidate;
            }
            send_stop(mpi_status.MPI_SOURCE);
            ++stopped_workers;
        } else if (mpi_status.MPI_TAG == TAG_ERROR) {
            int worker_error;

            MPI_Recv(
                &worker_error,
                1,
                MPI_INT,
                mpi_status.MPI_SOURCE,
                TAG_ERROR,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            );
            *valid = 0;
            send_stop(mpi_status.MPI_SOURCE);
            ++stopped_workers;
        } else {
            *valid = 0;
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }
}

static void run_worker(
    const unsigned char *ciphertext,
    size_t ciphertext_length,
    const unsigned char *phrase,
    size_t phrase_length,
    unsigned char *work_buffer,
    uint64_t *local_attempts
) {
    bool running = true;

    while (running) {
        int request = 1;
        uint64_t assignment[2] = {0U, 0U};
        MPI_Status mpi_status;

        MPI_Send(&request, 1, MPI_INT, 0, TAG_REQUEST, MPI_COMM_WORLD);
        MPI_Recv(
            assignment,
            2,
            MPI_UINT64_T,
            0,
            MPI_ANY_TAG,
            MPI_COMM_WORLD,
            &mpi_status
        );
        if (mpi_status.MPI_TAG == TAG_STOP) {
            break;
        }
        if (mpi_status.MPI_TAG != TAG_WORK) {
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        for (uint64_t key = assignment[0]; key < assignment[1]; ++key) {
            bool matches = false;
            Project2Status status = project2_try_key(
                key,
                ciphertext,
                ciphertext_length,
                phrase,
                phrase_length,
                work_buffer,
                ciphertext_length + 1U,
                &matches
            );

            ++*local_attempts;
            if (status != PROJECT2_OK) {
                int worker_error = (int)status;

                MPI_Send(&worker_error, 1, MPI_INT, 0, TAG_ERROR, MPI_COMM_WORLD);
                MPI_Recv(
                    assignment,
                    2,
                    MPI_UINT64_T,
                    0,
                    TAG_STOP,
                    MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE
                );
                running = false;
                break;
            }
            if (matches) {
                MPI_Send(&key, 1, MPI_UINT64_T, 0, TAG_FOUND, MPI_COMM_WORLD);
                MPI_Recv(
                    assignment,
                    2,
                    MPI_UINT64_T,
                    0,
                    TAG_STOP,
                    MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE
                );
                running = false;
                break;
            }
        }
    }
}

int main(int argc, char **argv) {
    int rank;
    int process_count;
    int valid = 1;
    Options options;
    unsigned char *ciphertext = NULL;
    unsigned char *phrase = NULL;
    unsigned char *work_buffer = NULL;
    uint64_t ciphertext_length = 0U;
    uint64_t phrase_length = 0U;
    uint64_t config[2] = {0U, 0U};
    uint64_t local_attempts = 0U;
    uint64_t total_attempts = 0U;
    uint64_t global_key = UINT64_MAX;
    double start_time;
    double local_elapsed;
    double elapsed = 0.0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &process_count);

    if (rank == 0) {
        size_t file_length = 0U;
        Project2Status status;

        valid = process_count >= 2 && parse_arguments(argc, argv, &options) ? 1 : 0;
        if (valid) {
            status = project2_read_file(options.input_path, &ciphertext, &file_length);
            valid = status == PROJECT2_OK && file_length <= INT_MAX &&
                    strlen(options.phrase) <= INT_MAX;
            if (!valid) {
                fprintf(stderr, "No se pudo preparar la entrada MPI.\n");
            } else {
                ciphertext_length = (uint64_t)file_length;
                phrase_length = (uint64_t)strlen(options.phrase);
                phrase = (unsigned char *)options.phrase;
                config[0] = options.max_key;
                config[1] = options.chunk_size;
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

    if (rank != 0) {
        work_buffer = malloc((size_t)ciphertext_length + 1U);
        valid = work_buffer != NULL;
    }
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

    MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();
    if (rank == 0) {
        run_master(
            process_count - 1,
            config[0],
            config[1],
            &global_key,
            &valid
        );
    } else {
        run_worker(
            ciphertext,
            (size_t)ciphertext_length,
            phrase,
            (size_t)phrase_length,
            work_buffer,
            &local_attempts
        );
    }

    MPI_Bcast(&valid, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&global_key, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
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
            "Modo: MPI dinamico master-worker\nProcesos: %d\nWorkers: %d\n"
            "Archivo: %s\nFrase conocida: %s\nRango global: [0, %" PRIu64 ")\n"
            "Tamano de bloque: %" PRIu64 "\nIntentos totales: %" PRIu64
            "\nTiempo: %.9f s\n",
            process_count,
            process_count - 1,
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
            size_t plaintext_length = 0U;
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
