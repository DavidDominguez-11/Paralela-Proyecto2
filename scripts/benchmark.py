#!/usr/bin/env python3

import argparse
import csv
import platform
import random
import re
import statistics
import subprocess
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DES_KEYSPACE = 1 << 56
TIME_PATTERN = re.compile(r"^Tiempo: ([0-9]+(?:\.[0-9]+)?) s$", re.MULTILINE)
ATTEMPTS_PATTERN = re.compile(
    r"^Intentos(?: totales)?: ([0-9]+)$", re.MULTILINE
)
KEY_PATTERN = re.compile(r"^Llave: ([0-9]+)$", re.MULTILINE)

RAW_FIELDS = [
    "timestamp_utc",
    "host",
    "git_revision",
    "git_dirty",
    "input_file",
    "input_bytes",
    "phrase",
    "key",
    "max_key",
    "strategy",
    "processes",
    "workers",
    "repetition",
    "parameter_name",
    "parameter_value",
    "attempts",
    "seconds",
    "found_key",
    "status",
]

SUMMARY_FIELDS = [
    "key",
    "max_key",
    "strategy",
    "processes",
    "workers",
    "parameter_name",
    "parameter_value",
    "samples",
    "median_attempts",
    "min_seconds",
    "median_seconds",
    "max_seconds",
    "speedup",
    "efficiency_total",
    "efficiency_workers",
]


def positive_int(text):
    value = int(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("se requiere un entero positivo")
    return value


def nonnegative_int(text):
    value = int(text)
    if value < 0:
        raise argparse.ArgumentTypeError("se requiere un entero no negativo")
    return value


def parse_number_list(text, label, minimum):
    try:
        values = [int(item.strip()) for item in text.split(",")]
    except ValueError as error:
        raise ValueError(f"{label} debe ser una lista de enteros") from error
    if not values or any(value < minimum for value in values):
        raise ValueError(f"{label} contiene un valor fuera de rango")
    return list(dict.fromkeys(values))


def run_command(command, *, show_output=False):
    result = subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        text=True,
        stdout=None if show_output else subprocess.PIPE,
        stderr=None if show_output else subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        stdout = result.stdout or ""
        stderr = result.stderr or ""
        raise RuntimeError(
            "fallo el comando: "
            + " ".join(str(part) for part in command)
            + f"\n{stdout}{stderr}"
        )
    return result.stdout or ""


def git_metadata():
    revision = "unknown"
    dirty = "unknown"
    try:
        revision = run_command(["git", "rev-parse", "--short", "HEAD"]).strip()
        dirty = "yes" if run_command(["git", "status", "--porcelain"]).strip() else "no"
    except (OSError, RuntimeError):
        pass
    return revision, dirty


def parse_result(output, expected_key):
    time_match = TIME_PATTERN.search(output)
    attempts_match = ATTEMPTS_PATTERN.search(output)
    key_match = KEY_PATTERN.search(output)
    if time_match is None or attempts_match is None or key_match is None:
        raise RuntimeError(f"salida de busqueda no reconocida:\n{output}")

    found_key = int(key_match.group(1))
    if found_key != expected_key or "Resultado: llave encontrada" not in output:
        raise RuntimeError(
            f"se esperaba la llave {expected_key}, pero la salida fue:\n{output}"
        )
    return int(attempts_match.group(1)), float(time_match.group(1)), found_key


def make_case(strategy, processes, check_interval, chunk_size):
    if strategy == "sequential":
        return {
            "strategy": strategy,
            "processes": 1,
            "workers": 1,
            "parameter_name": "",
            "parameter_value": 0,
        }
    if strategy == "dynamic":
        return {
            "strategy": strategy,
            "processes": processes,
            "workers": processes - 1,
            "parameter_name": "chunk_size",
            "parameter_value": chunk_size,
        }
    return {
        "strategy": strategy,
        "processes": processes,
        "workers": processes,
        "parameter_name": "check_interval",
        "parameter_value": check_interval,
    }


def search_command(case, cipher_path, phrase, max_key):
    common = [
        "--input",
        str(cipher_path),
        "--phrase",
        phrase,
        "--max-key",
        str(max_key),
    ]
    if case["strategy"] == "sequential":
        return [str(PROJECT_ROOT / "bin" / "bruteforce_seq"), *common]

    executable = {
        "naive": "bruteforce_mpi",
        "cyclic": "bruteforce_mpi_cyclic",
        "dynamic": "bruteforce_mpi_dynamic",
    }[case["strategy"]]
    option = (
        "--chunk-size" if case["strategy"] == "dynamic" else "--check-interval"
    )
    return [
        "mpirun",
        "-np",
        str(case["processes"]),
        str(PROJECT_ROOT / "bin" / executable),
        *common,
        option,
        str(case["parameter_value"]),
    ]


def write_summary(raw_rows, summary_path):
    groups = defaultdict(list)
    for row in raw_rows:
        group_key = (
            row["key"],
            row["max_key"],
            row["strategy"],
            row["processes"],
            row["workers"],
            row["parameter_name"],
            row["parameter_value"],
        )
        groups[group_key].append(row)

    sequential_medians = {}
    for group_key, rows in groups.items():
        if group_key[2] == "sequential":
            sequential_medians[group_key[0]] = statistics.median(
                row["seconds"] for row in rows
            )

    with summary_path.open("w", newline="", encoding="utf-8") as summary_file:
        writer = csv.DictWriter(summary_file, fieldnames=SUMMARY_FIELDS)
        writer.writeheader()
        for group_key in sorted(groups):
            rows = groups[group_key]
            seconds = [row["seconds"] for row in rows]
            attempts = [row["attempts"] for row in rows]
            median_seconds = statistics.median(seconds)
            baseline = sequential_medians[group_key[0]]
            speedup = baseline / median_seconds if median_seconds > 0.0 else 0.0
            processes = group_key[3]
            workers = group_key[4]
            writer.writerow(
                {
                    "key": group_key[0],
                    "max_key": group_key[1],
                    "strategy": group_key[2],
                    "processes": processes,
                    "workers": workers,
                    "parameter_name": group_key[5],
                    "parameter_value": group_key[6] or "",
                    "samples": len(rows),
                    "median_attempts": f"{statistics.median(attempts):.1f}",
                    "min_seconds": f"{min(seconds):.9f}",
                    "median_seconds": f"{median_seconds:.9f}",
                    "max_seconds": f"{max(seconds):.9f}",
                    "speedup": f"{speedup:.6f}",
                    "efficiency_total": f"{speedup / processes:.6f}",
                    "efficiency_workers": f"{speedup / workers:.6f}",
                }
            )


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Ejecuta y resume benchmarks reproducibles del Proyecto 2."
    )
    parser.add_argument("--input", default="data/mensaje.txt")
    parser.add_argument("--phrase", default="es una prueba de")
    parser.add_argument("--keys", default="1000,10000,50000")
    parser.add_argument("--max-key", type=positive_int, default=100000)
    parser.add_argument("--processes", default="2,4")
    parser.add_argument("--repetitions", type=positive_int, default=3)
    parser.add_argument("--warmups", type=nonnegative_int, default=1)
    parser.add_argument("--check-interval", type=positive_int, default=4096)
    parser.add_argument("--chunk-size", type=positive_int, default=4096)
    parser.add_argument("--seed", type=int, default=2026)
    parser.add_argument("--output-dir", default="build/benchmarks")
    parser.add_argument("--skip-build", action="store_true")
    return parser.parse_args()


def main():
    args = parse_arguments()
    keys = parse_number_list(args.keys, "--keys", 0)
    process_counts = parse_number_list(args.processes, "--processes", 2)
    if args.max_key > DES_KEYSPACE:
        raise ValueError("--max-key no puede exceder 2^56")
    if any(key >= args.max_key or key >= DES_KEYSPACE for key in keys):
        raise ValueError("cada llave debe pertenecer al rango [0, max-key)")

    input_path = (PROJECT_ROOT / args.input).resolve()
    if not input_path.is_file():
        raise ValueError(f"no existe el archivo de entrada: {input_path}")
    try:
        input_label = str(input_path.relative_to(PROJECT_ROOT))
    except ValueError:
        input_label = str(input_path)
    output_dir = (PROJECT_ROOT / args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    raw_path = output_dir / "raw.csv"
    summary_path = output_dir / "summary.csv"

    if not args.skip_build:
        run_command(["make", "all"], show_output=True)

    revision, dirty = git_metadata()
    host = platform.node()
    cases = [make_case("sequential", 1, args.check_interval, args.chunk_size)]
    for process_count in process_counts:
        cases.extend(
            make_case(strategy, process_count, args.check_interval, args.chunk_size)
            for strategy in ("naive", "cyclic", "dynamic")
        )

    raw_rows = []
    with raw_path.open("w", newline="", encoding="utf-8") as raw_file:
        writer = csv.DictWriter(raw_file, fieldnames=RAW_FIELDS)
        writer.writeheader()

        for key in keys:
            cipher_path = output_dir / f"key-{key}.des"
            run_command(
                [
                    str(PROJECT_ROOT / "bin" / "des_tool"),
                    "encrypt",
                    "--input",
                    str(input_path),
                    "--output",
                    str(cipher_path),
                    "--key",
                    str(key),
                ]
            )

            for case in cases:
                command = search_command(case, cipher_path, args.phrase, args.max_key)
                for _ in range(args.warmups):
                    parse_result(run_command(command), key)

            measured_runs = [
                (repetition, case)
                for repetition in range(1, args.repetitions + 1)
                for case in cases
            ]
            random.Random(args.seed + key).shuffle(measured_runs)
            for repetition, case in measured_runs:
                output = run_command(
                    search_command(case, cipher_path, args.phrase, args.max_key)
                )
                attempts, seconds, found_key = parse_result(output, key)
                row = {
                    "timestamp_utc": datetime.now(timezone.utc).isoformat(),
                    "host": host,
                    "git_revision": revision,
                    "git_dirty": dirty,
                    "input_file": input_label,
                    "input_bytes": input_path.stat().st_size,
                    "phrase": args.phrase,
                    "key": key,
                    "max_key": args.max_key,
                    "strategy": case["strategy"],
                    "processes": case["processes"],
                    "workers": case["workers"],
                    "repetition": repetition,
                    "parameter_name": case["parameter_name"],
                    "parameter_value": case["parameter_value"] or "",
                    "attempts": attempts,
                    "seconds": seconds,
                    "found_key": found_key,
                    "status": "found",
                }
                writer.writerow(row)
                raw_file.flush()
                raw_rows.append(row)

    write_summary(raw_rows, summary_path)
    print(f"Resultados individuales: {raw_path}")
    print(f"Resumen estadistico: {summary_path}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        raise SystemExit(f"ERROR: {error}") from error
