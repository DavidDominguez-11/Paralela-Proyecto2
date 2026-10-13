#!/usr/bin/env python3
"""Capture actual visible Windows console executions for the report.

Run from the project root on Windows with an interactive desktop. Each PNG is
captured from a newly launched console window after its commands finish.
"""

import ctypes
from ctypes import wintypes
import subprocess
import sys
import time
from pathlib import Path

from PIL import ImageGrab


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
EVIDENCE = ROOT / "evidence"
WSL = "wsl.exe -d Ubuntu-24.04 -- "


def continued(*parts):
    """Keep a real cmd.exe command readable without changing its arguments."""
    return "^\r\n    ".join(parts)


def mpi_command(program, input_path, max_key, option, value):
    return continued(
        WSL + f"mpirun -np 4 ./bin/{program} ",
        f"--input {input_path} ",
        '--phrase "es una prueba de" ',
        f"--max-key {max_key} {option} {value}",
    )


CASES = {
    "tests": [WSL + "make test-all"],
    "sanitizers": [WSL + "make test-sanitize"],
    "summaries": [WSL + "python3 scripts/show_results.py"],
    "historical_summary": [WSL + "python3 scripts/show_results.py historical"],
    "encrypt_decrypt_42": [
        continued(WSL + "./bin/des_tool encrypt ",
                  "--input data/mensaje.txt --output build/mensaje.des --key 42"),
        continued(WSL + "./bin/des_tool decrypt ",
                  "--input build/mensaje.des --output build/mensaje.dec.txt --key 42"),
        WSL + "cmp data/mensaje.txt build/mensaje.dec.txt",
        continued(WSL + "./bin/bruteforce_seq ",
                  '--input build/mensaje.des --phrase "es una prueba de" --max-key 1000'),
    ],
    "mpi_naive": [
        mpi_command("bruteforce_mpi", "build/mensaje.des", 1000,
                    "--check-interval", 16),
    ],
    "mpi_cyclic": [
        mpi_command("bruteforce_mpi_cyclic", "build/mensaje.des", 1000,
                    "--check-interval", 16),
    ],
    "mpi_dynamic": [
        mpi_command("bruteforce_mpi_dynamic", "build/mensaje.des", 1000,
                    "--chunk-size", 16),
    ],
    "key_123456": [
        mpi_command("bruteforce_mpi", "results/required-123456/key-123456.des",
                    72057594037927936, "--check-interval", 4096),
    ],
    "quarter_first": [
        mpi_command("bruteforce_mpi",
                    "results/required-boundary/key-18014398509481984.des",
                    72057594037927936, "--check-interval", 4096),
    ],
    "quarter_minus_one_prefix": [
        mpi_command("bruteforce_mpi",
                    "results/required-boundary/key-18014398509481983.des",
                    1000000, "--check-interval", 4096),
    ],
}


def prepare_fixtures():
    for key, path in (
        (42, ROOT / "build" / "mensaje.des"),
        (123456, ROOT / "results" / "required-123456" / "key-123456.des"),
        (18014398509481983, ROOT / "results" / "required-boundary" /
         "key-18014398509481983.des"),
        (18014398509481984, ROOT / "results" / "required-boundary" /
         "key-18014398509481984.des"),
    ):
        if path.is_file():
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        result = subprocess.run([
            "wsl.exe", "-d", "Ubuntu-24.04", "--", "./bin/des_tool", "encrypt",
            "--input", "data/mensaje.txt", "--output",
            str(path.relative_to(ROOT)).replace("\\", "/"), "--key", str(key),
        ], cwd=ROOT, capture_output=True, text=True, check=False, timeout=30)
        if result.returncode != 0:
            raise RuntimeError(f"no se pudo preparar {path.name}: {result.stderr}")


def capture(name, commands):
    title = f"Proyecto2-evidencia-{name}"
    ready = BUILD / f"capture-{name}.ready"
    cmd_path = BUILD / f"capture-{name}.cmd"
    ready.unlink(missing_ok=True)
    lines = ["@echo off", "mode con: cols=100 lines=44", f"title {title}"]
    for command in commands:
        displayed_parts = command.split("^\r\n    ")
        for index, part in enumerate(displayed_parts):
            lines.append("echo(" + ("$ " if index == 0 else "  ") + part.rstrip())
        lines.append(command)
        if name == "quarter_minus_one_prefix":
            # No match in the declared prefix is the expected exit status.
            lines.append("if not errorlevel 1 exit /b 1")
        else:
            lines.append("if errorlevel 1 exit /b 1")
    lines.extend([
        f"echo completed > \"{ready}\"", "ping -n 31 127.0.0.1 >nul",
    ])
    cmd_path.write_text("\r\n".join(lines) + "\r\n", encoding="ascii")
    process = subprocess.Popen(
        ["cmd.exe", "/c", str(cmd_path)], cwd=ROOT,
        creationflags=subprocess.CREATE_NEW_CONSOLE,
    )
    try:
        deadline = time.monotonic() + 120.0
        user32 = ctypes.windll.user32
        user32.FindWindowW.restype = ctypes.c_void_p
        user32.FindWindowW.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p]
        hwnd = 0
        while time.monotonic() < deadline:
            hwnd = user32.FindWindowW(None, title)
            if ready.exists() and hwnd:
                break
            if process.poll() is not None:
                raise RuntimeError(f"la consola {name} termino antes de la captura")
            time.sleep(0.25)
        else:
            raise TimeoutError(f"la consola {name} no estuvo lista")
        user32.ShowWindow(hwnd, 9)
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.5)
        bounds = wintypes.RECT()
        if not user32.GetWindowRect(hwnd, ctypes.byref(bounds)):
            raise RuntimeError(f"no se pudo ubicar la consola {name}")
        image = ImageGrab.grab(bbox=(
            bounds.left, bounds.top, bounds.right, bounds.bottom,
        ))
        if image.getbbox() is None:
            raise RuntimeError(f"la captura {name} esta vacia")
        destination = EVIDENCE / f"{name}.png"
        image.save(destination)
        print(f"{destination} ({image.width}x{image.height})")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
        ready.unlink(missing_ok=True)
        cmd_path.unlink(missing_ok=True)


def main():
    if sys.platform != "win32":
        raise SystemExit("Este capturador requiere un escritorio Windows visible.")
    BUILD.mkdir(exist_ok=True)
    EVIDENCE.mkdir(exist_ok=True)
    prepare_fixtures()
    requested = sys.argv[1:] or list(CASES)
    for name in requested:
        if name not in CASES:
            raise SystemExit(f"Evidencia desconocida: {name}")
        capture(name, CASES[name])


if __name__ == "__main__":
    main()
