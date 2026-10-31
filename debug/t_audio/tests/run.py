"""Host checks for the signal generation and recording meter used by t_audio."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
compiler = os.environ.get("CC", "gcc")
if not shutil.which(compiler):
    raise SystemExit(f"C compiler not found: {compiler}")
with tempfile.TemporaryDirectory(prefix="bepio-audio-tests-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(HERE.parent / "main"), str(HERE / "main.c"),
        str(HERE.parent / "main/audio_signal.c"), "-lm", "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
