#!/usr/bin/env python3
"""Measure the two literal DES quarter-boundary cases from the assignment.

The last-key case searches only a declared prefix; it is never described as a
completed search of the full 56-bit space. Every subprocess has a time limit.
"""

import argparse
import csv
import hashlib
import platform
import re
import shlex
import statistics
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
KEYSPACE = 1 << 56
QUARTER = KEYSPACE // 4
PHRASE = "es una prueba de"
INPUT = ROOT / "data" / "mensaje.txt"
EXE = ROOT / "bin" / "bruteforce_mpi"
TIME_RE = re.compile(r"^Tiempo: ([0-9.]+) s$", re.MULTILINE)
ATTEMPTS_RE = re.compile(r"^Intentos totales: ([0-9]+)$", re.MULTILINE)
KEY_RE = re.compile(r"^Llave: ([0-9]+)$", re.MULTILINE)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(command, timeout_seconds):
    started = time.perf_counter()
    completed = subprocess.run(
        command, cwd=ROOT, text=True, capture_output=True,
        timeout=timeout_seconds, check=False,
    )
    return completed, time.perf_counter() - started


def git_value(*arguments):
    result = subprocess.run(
        ["git", *arguments], cwd=ROOT, text=True,
        capture_output=True, check=False,
    )
    return result.stdout.strip() if result.returncode == 0 else "unknown"


def parse_search(completed, key, prefix_case, prefix_limit):
    output = completed.stdout
    seconds = TIME_RE.search(output)
    attempts = ATTEMPTS_RE.search(output)
    found = KEY_RE.search(output)
    if seconds is None or attempts is None:
        raise RuntimeError(f"salida incompleta:\n{output}\n{completed.stderr}")
    count = int(attempts.group(1))
    if prefix_case:
        if completed.returncode == 0 or found is not None or count != prefix_limit:
            raise RuntimeError(f"muestra acotada inesperada:\n{output}\n{completed.stderr}")
        if "Resultado: llave no encontrada" not in output:
            raise RuntimeError(f"falta el estado de agotamiento:\n{output}")
        status = "not_found_in_prefix"
        found_key = ""
    else:
        if completed.returncode != 0 or found is None or int(found.group(1)) != key:
            raise RuntimeError(f"no se recupero la llave {key}:\n{output}\n{completed.stderr}")
        status = "found"
        found_key = key
    return count, float(seconds.group(1)), found_key, status


def median(values):
    return statistics.median(values)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", default="results/required-boundary")
    parser.add_argument("--prefix-max-key", type=int, default=1_000_000)
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument("--warmups", type=int, default=1)
    parser.add_argument("--check-interval", type=int, default=4096)
    parser.add_argument("--timeout-seconds", type=float, default=30.0)
    args = parser.parse_args()
    if not (0 < args.prefix_max_key < QUARTER):
        parser.error("--prefix-max-key debe estar entre 1 y 2^54 - 1")
    if (args.repetitions <= 0 or args.warmups < 0 or
            args.check_interval <= 0 or args.timeout_seconds <= 0):
        parser.error("repetitions, check-interval y timeout deben ser positivos")
    if not INPUT.is_file() or not EXE.is_file():
        parser.error("faltan la entrada o bin/bruteforce_mpi; ejecute make all")

    output_dir = (ROOT / args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    revision = git_value("rev-parse", "--short", "HEAD")
    dirty = "yes" if git_value("status", "--porcelain") else "no"
    rows = []
    fields = [
        "timestamp_utc", "host", "git_revision", "git_dirty", "binary_sha256",
        "input_sha256", "cipher_sha256", "case", "target_key", "max_key",
        "processes", "check_interval", "repetition", "attempts",
        "search_seconds", "wall_seconds", "found_key", "status", "return_code",
        "command", "log_file",
    ]
    raw_path = output_dir / "raw.csv"
    with raw_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for case, key, max_key, prefix_case in (
            ("quarter_minus_one_prefix", QUARTER - 1, args.prefix_max_key, True),
            ("quarter_first_full", QUARTER, KEYSPACE, False),
        ):
            cipher = output_dir / f"key-{key}.des"
            encrypt = [
                str(ROOT / "bin" / "des_tool"), "encrypt", "--input", str(INPUT),
                "--output", str(cipher), "--key", str(key),
            ]
            encrypted, _ = execute(encrypt, args.timeout_seconds)
            if encrypted.returncode != 0:
                raise RuntimeError(f"no se pudo cifrar {key}: {encrypted.stderr}")
            (output_dir / f"encrypt-{key}.log").write_text(
                "$ " + shlex.join(encrypt) + "\n" + encrypted.stdout + encrypted.stderr,
                encoding="utf-8",
            )
            command = [
                "mpirun", "-np", "4", str(EXE), "--input", str(cipher),
                "--phrase", PHRASE, "--max-key", str(max_key),
                "--check-interval", str(args.check_interval),
            ]
            for repetition in range(1 - args.warmups, args.repetitions + 1):
                completed, wall_seconds = execute(command, args.timeout_seconds)
                attempts, search_seconds, found_key, status = parse_search(
                    completed, key, prefix_case, args.prefix_max_key,
                )
                name = f"{case}-{'warmup' if repetition <= 0 else repetition}.log"
                (output_dir / name).write_text(
                    "$ " + shlex.join(command) + "\n" + completed.stdout +
                    completed.stderr + f"[return_code={completed.returncode}]\n",
                    encoding="utf-8",
                )
                if repetition <= 0:
                    continue
                row = {
                    "timestamp_utc": datetime.now(timezone.utc).isoformat(),
                    "host": platform.node(), "git_revision": revision,
                    "git_dirty": dirty, "binary_sha256": digest(EXE),
                    "input_sha256": digest(INPUT), "cipher_sha256": digest(cipher),
                    "case": case, "target_key": key, "max_key": max_key,
                    "processes": 4, "check_interval": args.check_interval,
                    "repetition": repetition, "attempts": attempts,
                    "search_seconds": f"{search_seconds:.9f}",
                    "wall_seconds": f"{wall_seconds:.9f}",
                    "found_key": found_key, "status": status,
                    "return_code": completed.returncode,
                    "command": shlex.join(command), "log_file": name,
                }
                writer.writerow(row)
                stream.flush()
                rows.append(row)

    summary_path = output_dir / "summary.csv"
    with summary_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow([
            "case", "target_key", "max_key", "samples", "median_attempts",
            "median_search_seconds", "median_wall_seconds", "status",
        ])
        for case in ("quarter_minus_one_prefix", "quarter_first_full"):
            group = [row for row in rows if row["case"] == case]
            writer.writerow([
                case, group[0]["target_key"], group[0]["max_key"], len(group),
                median([row["attempts"] for row in group]),
                f"{median([float(row['search_seconds']) for row in group]):.9f}",
                f"{median([float(row['wall_seconds']) for row in group]):.9f}",
                group[0]["status"],
            ])
    print(f"Resultados individuales: {raw_path}")
    print(f"Resumen: {summary_path}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        raise SystemExit(f"ERROR: {error}") from error
