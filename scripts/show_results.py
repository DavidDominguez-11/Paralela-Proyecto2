#!/usr/bin/env python3
"""Print the measured literal cases in a compact, screenshot-friendly table."""

import csv
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def rows(path):
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


section = sys.argv[1] if len(sys.argv) > 1 else "new"
if section not in {"new", "historical", "all"}:
    raise SystemExit("Uso: show_results.py [new|historical|all]")

if section in {"historical", "all"}:
    print("Campana historica M=1000000; MPI con 4 procesos; speedup mediano")
    print("Llave    Estrategia    Intentos       Tiempo (s)    Speedup")
    for row in rows(ROOT / "results/final-campaign/summary.csv"):
        if row["processes"] == "4" and row["strategy"] != "sequential":
            print(f"{row['key']:>6}    {row['strategy']:<10} "
                  f"{row['median_attempts']:>10} {row['median_seconds']:>15} "
                  f"{row['speedup']:>10}")

if section in {"new", "all"}:
    print("Llave 123456; medianas de 5 mediciones; region de busqueda")
    print("Estrategia   Procesos   Intentos    Tiempo (s)   Speedup")
    for row in rows(ROOT / "results/required-123456/summary.csv"):
        print(f"{row['strategy']:<12} {row['processes']:>8} "
              f"{row['median_attempts']:>10} {row['median_seconds']:>13} "
              f"{row['speedup']:>9}")

    print("\nLimite 2^56/4; medianas de 5 mediciones; region de busqueda")
    print("Caso                 Intentos    Tiempo (s)   Estado")
    for row in rows(ROOT / "results/required-boundary/summary.csv"):
        print(f"{row['case']:<20} {row['median_attempts']:>10} "
              f"{row['median_search_seconds']:>13}   {row['status']}")

    print("\nBusqueda secuencial por cantidad de digitos; medianas de 5 mediciones")
    print("Digitos    Llave   Intentos   Busqueda (s)")
    for row in rows(ROOT / "results/key-lengths/summary.csv"):
        print(f"{row['decimal_digits']:>7} {row['key']:>8} {row['attempts']:>10} "
              f"{row['median_search_seconds']:>14}")
