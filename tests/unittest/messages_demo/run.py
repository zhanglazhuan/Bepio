"""Verify demo conversations appear alongside saved messages and do not duplicate."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
with tempfile.TemporaryDirectory(prefix="bepio-messages-demo-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run([
        os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
        "-include", str(ROOT / "tests/unittest/messages/host/errno_compat.h"),
        "-DCONFIG_BEPIO_MESSAGES_INBOX_CAPACITY=20",
        "-DCONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY=20",
        "-I", str(ROOT / "tests/unittest/app_manager/host"),
        "-I", str(ROOT / "system/services/messaging"),
        "-I", str(ROOT / "apps/messages"),
        str(HERE / "main.c"), str(ROOT / "system/services/messaging/messaging.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
