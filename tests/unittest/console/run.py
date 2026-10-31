"""Compile the actual ESP-IDF console adapter with only SDK entrypoints mocked."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
compiler = os.environ.get("CC", "gcc")
if not shutil.which(compiler):
    raise SystemExit(f"C compiler not found: {compiler}")
with tempfile.TemporaryDirectory(prefix="bepio-console-tests-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(HERE / "host"), "-I", str(ROOT / "system/platform"),
        str(HERE / "main.c"), str(ROOT / "system/platform/bepio_console.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
