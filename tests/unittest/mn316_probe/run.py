"""Exercise the real MN316 probe against a deterministic UART/clock mock."""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
with tempfile.TemporaryDirectory(prefix="bepio-mn316-probe-") as directory:
    executable = Path(directory) / "probe.exe"
    subprocess.run([
        os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(here / "host"), "-I", str(root / "drivers/mn316"),
        str(here / "main.c"), str(root / "drivers/mn316/mn316.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
