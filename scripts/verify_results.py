#!/usr/bin/env python3
"""Independently verify historical and literal-case CSV summaries."""

import csv
import statistics
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read_rows(path):
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def close(actual, expected, tolerance=0.000002):
    if abs(float(actual) - float(expected)) > tolerance:
        raise AssertionError(f"{actual} != {expected}")


def check_benchmark(directory, expected_rows, expected_groups):
    raw = read_rows(directory / "raw.csv")
    summary = read_rows(directory / "summary.csv")
    assert len(raw) == expected_rows
    assert len(summary) == expected_groups
    groups = defaultdict(list)
    for row in raw:
        assert row["status"] == "found" and row["found_key"] == row["key"]
        key = tuple(row[field] for field in (
            "key", "max_key", "strategy", "processes", "workers",
            "parameter_name", "parameter_value",
        ))
        groups[key].append(row)
    baselines = {
        key[0]: statistics.median(float(row["seconds"]) for row in rows)
        for key, rows in groups.items() if key[2] == "sequential"
    }
    assert len(groups) == len(summary)
    for item in summary:
        key = tuple(item[field] for field in (
            "key", "max_key", "strategy", "processes", "workers",
            "parameter_name", "parameter_value",
        ))
        rows = groups[key]
        times = [float(row["seconds"]) for row in rows]
        attempts = [int(row["attempts"]) for row in rows]
        assert len(rows) == int(item["samples"])
        close(item["median_attempts"], statistics.median(attempts))
        close(item["min_seconds"], min(times), 1e-9)
        close(item["median_seconds"], statistics.median(times), 1e-9)
        close(item["max_seconds"], max(times), 1e-9)
        speedup = baselines[item["key"]] / statistics.median(times)
        close(item["speedup"], speedup)
        close(item["efficiency_total"], speedup / int(item["processes"]))
        close(item["efficiency_workers"], speedup / int(item["workers"]))
    print(f"OK: {directory.relative_to(ROOT)}: {len(raw)} filas, {len(summary)} grupos")


def check_boundary(directory):
    raw = read_rows(directory / "raw.csv")
    summary = read_rows(directory / "summary.csv")
    assert len(raw) == 10 and len(summary) == 2
    groups = defaultdict(list)
    for row in raw:
        assert int(row["processes"]) == 4
        assert (directory / row["log_file"]).is_file()
        if row["case"] == "quarter_first_full":
            assert row["status"] == "found"
            assert row["found_key"] == row["target_key"]
            assert int(row["max_key"]) == 1 << 56
        else:
            assert row["status"] == "not_found_in_prefix"
            assert row["found_key"] == ""
            assert int(row["attempts"]) == int(row["max_key"])
        groups[row["case"]].append(row)
    for item in summary:
        rows = groups[item["case"]]
        assert len(rows) == int(item["samples"]) == 5
        assert all(row["status"] == item["status"] for row in rows)
        close(item["median_attempts"], statistics.median(
            int(row["attempts"]) for row in rows
        ))
        close(item["median_search_seconds"], statistics.median(
            float(row["search_seconds"]) for row in rows
        ), 1e-9)
        close(item["median_wall_seconds"], statistics.median(
            float(row["wall_seconds"]) for row in rows
        ), 1e-9)
    print(f"OK: {directory.relative_to(ROOT)}: {len(raw)} filas, {len(summary)} grupos")


def check_key_lengths(directory):
    raw = read_rows(directory / "raw.csv")
    summary = read_rows(directory / "summary.csv")
    assert len(raw) == 30 and len(summary) == 6
    groups = defaultdict(list)
    for row in raw:
        assert row["status"] == "found" and row["found_key"] == row["key"]
        assert int(row["attempts"]) == int(row["key"]) + 1
        assert int(row["decimal_digits"]) == len(row["key"])
        groups[row["key"]].append(row)
    for item in summary:
        rows = groups[item["key"]]
        assert len(rows) == int(item["samples"]) == 5
        assert int(item["attempts"]) == int(item["key"]) + 1
        times = [float(row["search_seconds"]) for row in rows]
        close(item["min_search_seconds"], min(times), 1e-9)
        close(item["median_search_seconds"], statistics.median(times), 1e-9)
        close(item["max_search_seconds"], max(times), 1e-9)
        close(item["median_wall_seconds"], statistics.median(
            float(row["wall_seconds"]) for row in rows
        ), 1e-9)
    print(f"OK: {directory.relative_to(ROOT)}: {len(raw)} filas, {len(summary)} grupos")


if __name__ == "__main__":
    check_benchmark(ROOT / "results" / "final-campaign", 105, 21)
    check_benchmark(ROOT / "results" / "required-123456", 20, 4)
    check_boundary(ROOT / "results" / "required-boundary")
    check_key_lengths(ROOT / "results" / "key-lengths")
