"""Exercise Chats real-data and RAM-only demo selection through its controller."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
with tempfile.TemporaryDirectory(prefix="bepio-chats-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    for demo, source in ((True, "main.c"), (False, "main_production.c")):
        subprocess.run([
            os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
            "-DCONFIG_BEPIO_CONTACTS_CAPACITY=32",
            "-DCONFIG_BEPIO_MESSAGES_INBOX_CAPACITY=20",
            "-DCONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY=20",
            *(["-DCONFIG_BEPIO_MESSAGES_DEMO=1"] if demo else []),
            "-I", str(ROOT), "-I", str(ROOT / "apps/chats"),
            "-I", str(ROOT / "tests/unittest/app_manager/host"),
            "-I", str(ROOT / "system/services/messaging"),
            str(HERE / source), str(ROOT / "apps/chats/model.c"),
            str(ROOT / "apps/chats/controller.c"), "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)
