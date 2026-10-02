#!/usr/bin/env bash

set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_root"

test_dir=$(mktemp -d "${TMPDIR:-/tmp}/project2-mpi.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

expect_failure() {
    if "$@" >"$test_dir/failure.log" 2>&1; then
        echo "ERROR: el comando debio fallar: $*" >&2
        return 1
    fi
}

./bin/des_tool encrypt \
    --input data/mensaje.txt \
    --output "$test_dir/key0.des" \
    --key 0 >"$test_dir/encrypt0.log"
mpirun -np 4 ./bin/bruteforce_mpi \
    --input "$test_dir/key0.des" \
    --phrase "es una prueba de" \
    --max-key 2 \
    --check-interval 1 >"$test_dir/more-processes.log"
grep -q "Llave: 0" "$test_dir/more-processes.log"

./bin/des_tool encrypt \
    --input data/mensaje.txt \
    --output "$test_dir/key5.des" \
    --key 5 >"$test_dir/encrypt5.log"
mpirun -np 3 ./bin/bruteforce_mpi \
    --input "$test_dir/key5.des" \
    --phrase "es una prueba de" \
    --max-key 10 \
    --check-interval 1 >"$test_dir/remainder.log"
grep -q "Llave: 5" "$test_dir/remainder.log"

mpirun -np 3 ./bin/bruteforce_mpi \
    --input "$test_dir/key5.des" \
    --phrase "es una prueba de" \
    --max-key 10 \
    --check-interval 4 >"$test_dir/interval4.log"
grep -q "Llave: 5" "$test_dir/interval4.log"

mpirun -np 4 ./bin/bruteforce_mpi_cyclic \
    --input "$test_dir/key0.des" \
    --phrase "es una prueba de" \
    --max-key 2 \
    --check-interval 1 >"$test_dir/cyclic-more-processes.log"
grep -q "Modo: MPI ciclico" "$test_dir/cyclic-more-processes.log"
grep -q "Llave: 0" "$test_dir/cyclic-more-processes.log"

mpirun -np 3 ./bin/bruteforce_mpi_cyclic \
    --input "$test_dir/key5.des" \
    --phrase "es una prueba de" \
    --max-key 10 \
    --check-interval 1 >"$test_dir/cyclic-remainder.log"
grep -q "Llave: 5" "$test_dir/cyclic-remainder.log"

mpirun -np 3 ./bin/bruteforce_mpi_cyclic \
    --input "$test_dir/key5.des" \
    --phrase "es una prueba de" \
    --max-key 10 \
    --check-interval 4 >"$test_dir/cyclic-interval4.log"
grep -q "Llave: 5" "$test_dir/cyclic-interval4.log"

expect_failure mpirun -np 3 ./bin/bruteforce_mpi \
    --input "$test_dir/key5.des" \
    --phrase "esta frase definitivamente no aparece" \
    --max-key 10 \
    --check-interval 2
grep -q "llave no encontrada" "$test_dir/failure.log"

expect_failure mpirun -np 3 ./bin/bruteforce_mpi_cyclic \
    --input "$test_dir/key5.des" \
    --phrase "esta frase definitivamente no aparece" \
    --max-key 10 \
    --check-interval 2
grep -q "llave no encontrada" "$test_dir/failure.log"

expect_failure mpirun -np 2 ./bin/bruteforce_mpi \
    --input "$test_dir/key5.des" \
    --phrase prueba \
    --max-key 10 \
    --check-interval " -1"

echo "OK: particiones, intervalos, llave cero y errores MPI."
