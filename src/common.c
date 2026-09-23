#define _POSIX_C_SOURCE 200809L

#include "project2/common.h"

#include <errno.h>
#include <stdlib.h>
#include <time.h>

const char *project2_status_message(Project2Status status) {
    switch (status) {
        case PROJECT2_OK:
            return "operacion exitosa";
        case PROJECT2_ERR_ARGUMENT:
            return "argumento invalido";
        case PROJECT2_ERR_IO:
            return "error de entrada/salida";
        case PROJECT2_ERR_MEMORY:
            return "memoria insuficiente";
        case PROJECT2_ERR_CRYPTO:
            return "error criptografico o padding invalido";
        case PROJECT2_ERR_NOT_FOUND:
            return "llave no encontrada en el rango";
        default:
            return "error desconocido";
    }
}

bool project2_parse_u64(const char *text, uint64_t *value) {
    char *end = NULL;
    unsigned long long parsed;

    if (text == NULL || value == NULL || *text == '\0' || *text == '-') {
        return false;
    }

    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }

    *value = (uint64_t)parsed;
    return true;
}

double project2_monotonic_seconds(void) {
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0.0;
    }

    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

