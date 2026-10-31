"""Exercise actual message snapshot persistence with an in-memory Flash adapter."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
with tempfile.TemporaryDirectory(prefix="bepio-messages-storage-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run([
        os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-include", str(ROOT / "tests/unittest/messages/host/errno_compat.h"),
        "-DBEPIO_MESSAGES_STORAGE=1", "-DCONFIG_BEPIO_MESSAGES_INBOX_CAPACITY=4",
        "-DCONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY=4",
        "-I", str(ROOT / "tests/unittest/app_manager/host"),
        "-I", str(ROOT / "system/services/messaging"),
        "-I", str(ROOT / "system/platform"),
        str(HERE / "main.c"), str(ROOT / "system/services/messaging/messaging.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
