#!/usr/bin/env bash

set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_root"

test_dir=$(mktemp -d "${TMPDIR:-/tmp}/project2-benchmark.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

python3 scripts/benchmark.py \
    --skip-build \
    --keys 5 \
    --max-key 10 \
    --processes 2 \
    --repetitions 1 \
    --warmups 0 \
    --check-interval 2 \
    --chunk-size 2 \
    --output-dir "$test_dir/results" >"$test_dir/benchmark.log"

raw_csv="$test_dir/results/raw.csv"
summary_csv="$test_dir/results/summary.csv"
test -f "$raw_csv"
test -f "$summary_csv"
test "$(wc -l <"$raw_csv")" -eq 5
test "$(wc -l <"$summary_csv")" -eq 5

for strategy in sequential naive cyclic dynamic; do
    grep -q ",$strategy," "$raw_csv"
    grep -q ",$strategy," "$summary_csv"
done

awk -F, '
    NR > 1 {
        gsub(/\r/, "", $19)
        if ($18 != 5 || $19 != "found") {
            exit 1
        }
    }
' "$raw_csv"
grep -q "speedup,efficiency_total,efficiency_workers" "$summary_csv"

echo "OK: CSV crudo, resumen, speedup y eficiencia."
