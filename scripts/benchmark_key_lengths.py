#!/usr/bin/env python3
"""Measure sequential search for keys with one through six decimal digits."""

import csv
import platform
import re
import statistics
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "results" / "key-lengths"
INPUT = ROOT / "data" / "mensaje.txt"
KEYS = (9, 99, 999, 9999, 99999, 123456)
MAX_KEY = 200000
PHRASE = "es una prueba de"
TIME_RE = re.compile(r"^Tiempo: ([0-9.]+) s$", re.MULTILINE)
ATTEMPTS_RE = re.compile(r"^Intentos: ([0-9]+)$", re.MULTILINE)
KEY_RE = re.compile(r"^Llave: ([0-9]+)$", re.MULTILINE)


def run(command):
    started = time.perf_counter()
    result = subprocess.run(
        command, cwd=ROOT, text=True, capture_output=True, check=False,
        timeout=30,
    )
    return result, time.perf_counter() - started


def git(command):
    result = subprocess.run(
        ["git", *command], cwd=ROOT, text=True, capture_output=True,
        check=False,
    )
    return result.stdout.strip() if result.returncode == 0 else "unknown"


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    revision = git(["rev-parse", "--short", "HEAD"])
    dirty = "yes" if git(["status", "--porcelain"]) else "no"
    fields = [
        "timestamp_utc", "host", "git_revision", "git_dirty", "key",
        "decimal_digits", "max_key", "repetition", "attempts",
        "search_seconds", "wall_seconds", "found_key", "status",
    ]
    rows = []
    with (OUTPUT / "raw.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for key in KEYS:
            cipher = OUTPUT / f"key-{key}.des"
            encrypted, _ = run([
                str(ROOT / "bin" / "des_tool"), "encrypt", "--input", str(INPUT),
                "--output", str(cipher), "--key", str(key),
            ])
            if encrypted.returncode != 0:
                raise RuntimeError(f"fallo el cifrado de {key}: {encrypted.stderr}")
            command = [
                str(ROOT / "bin" / "bruteforce_seq"), "--input", str(cipher),
                "--phrase", PHRASE, "--max-key", str(MAX_KEY),
            ]
            for repetition in range(0, 6):
                result, wall_seconds = run(command)
                attempts = ATTEMPTS_RE.search(result.stdout)
                seconds = TIME_RE.search(result.stdout)
                found = KEY_RE.search(result.stdout)
                if (result.returncode != 0 or attempts is None or seconds is None or
                        found is None or int(found.group(1)) != key or
                        int(attempts.group(1)) != key + 1):
                    raise RuntimeError(f"busqueda inesperada de {key}:\n"
                                       f"{result.stdout}\n{result.stderr}")
                if repetition == 0:  # calentamiento
                    continue
                row = {
                    "timestamp_utc": datetime.now(timezone.utc).isoformat(),
                    "host": platform.node(), "git_revision": revision,
                    "git_dirty": dirty, "key": key, "decimal_digits": len(str(key)),
                    "max_key": MAX_KEY, "repetition": repetition,
                    "attempts": int(attempts.group(1)),
                    "search_seconds": seconds.group(1),
                    "wall_seconds": f"{wall_seconds:.9f}",
                    "found_key": key, "status": "found",
                }
                writer.writerow(row)
                stream.flush()
                rows.append(row)

    with (OUTPUT / "summary.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow([
            "key", "decimal_digits", "samples", "attempts",
            "min_search_seconds", "median_search_seconds", "max_search_seconds",
            "median_wall_seconds",
        ])
        for key in KEYS:
            group = [row for row in rows if row["key"] == key]
            seconds = [float(row["search_seconds"]) for row in group]
            wall = [float(row["wall_seconds"]) for row in group]
            writer.writerow([
                key, len(str(key)), len(group), key + 1,
                f"{min(seconds):.9f}", f"{statistics.median(seconds):.9f}",
                f"{max(seconds):.9f}", f"{statistics.median(wall):.9f}",
            ])
    print(f"Resultados: {OUTPUT / 'raw.csv'}")
    print(f"Resumen: {OUTPUT / 'summary.csv'}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        raise SystemExit(f"ERROR: {error}") from error
