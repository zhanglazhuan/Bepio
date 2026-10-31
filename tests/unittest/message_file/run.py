"""Verify the production text codec and legacy hex snapshot compatibility."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
with tempfile.TemporaryDirectory(prefix="bepio-message-file-") as directory:
    target = Path(directory).as_posix()
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    compat = ["-include", "io.h", "-Dfsync=_commit"] if os.name == "nt" else []
    subprocess.run([
        os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        *compat,
        f'-DBEPIO_MESSAGE_FILE_PATH="{target}/messages.txt"',
        f'-DBEPIO_MESSAGE_TEMP_PATH="{target}/messages.tmp"',
        "-I", str(HERE / "host"), "-I", str(ROOT / "system/platform"),
        str(HERE / "main.c"), str(ROOT / "system/platform/bepio_message_file.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable), str(Path(directory) / "messages.txt")], check=True)
