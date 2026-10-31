"""Exercise Chat's real-data and RAM-only demo selection through its controller."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
with tempfile.TemporaryDirectory(prefix="bepio-chat-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run([
        os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
        "-DCONFIG_BEPIO_CONTACTS_CAPACITY=32",
        "-DCONFIG_BEPIO_MESSAGES_INBOX_CAPACITY=20",
        "-DCONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY=20",
        "-I", str(ROOT), "-I", str(ROOT / "apps/chat"),
        "-I", str(ROOT / "system/services/messaging"),
        str(HERE / "main.c"), str(ROOT / "apps/chat/model.c"),
        str(ROOT / "apps/chat/controller.c"), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
