#!/usr/bin/env bash

set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_root"

test_dir=$(mktemp -d "${TMPDIR:-/tmp}/project2-cli.XXXXXX")
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
./bin/des_tool decrypt \
    --input "$test_dir/key0.des" \
    --output "$test_dir/key0.txt" \
    --key 0 >"$test_dir/decrypt0.log"
cmp data/mensaje.txt "$test_dir/key0.txt"

./bin/bruteforce_seq \
    --input "$test_dir/key0.des" \
    --phrase "es una prueba de" \
    --max-key 2 >"$test_dir/search0.log"
grep -q "Llave: 0" "$test_dir/search0.log"

./bin/des_tool encrypt \
    --input data/mensaje.txt \
    --output "$test_dir/max.des" \
    --key 72057594037927935 >"$test_dir/encrypt-max.log"
./bin/des_tool decrypt \
    --input "$test_dir/max.des" \
    --output "$test_dir/max.txt" \
    --key 72057594037927935 >"$test_dir/decrypt-max.log"
cmp data/mensaje.txt "$test_dir/max.txt"

expect_failure ./bin/des_tool encrypt \
    --input data/mensaje.txt \
    --output "$test_dir/invalid.des" \
    --key 72057594037927936
expect_failure ./bin/des_tool encrypt \
    --input data/mensaje.txt \
    --output "$test_dir/negative.des" \
    --key " -1"
expect_failure ./bin/bruteforce_seq \
    --input "$test_dir/key0.des" \
    --phrase inexistente \
    --max-key 8
grep -q "llave no encontrada" "$test_dir/failure.log"
expect_failure ./bin/bruteforce_seq \
    --input "$test_dir/key0.des" \
    --phrase prueba \
    --start-key 10 \
    --max-key 10

echo "OK: comandos, llaves limite y resultados de error."
