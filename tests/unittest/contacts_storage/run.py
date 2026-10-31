"""Exercise the production Contacts snapshot codec with an in-memory Flash adapter."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
with tempfile.TemporaryDirectory(prefix="bepio-contacts-storage-") as directory:
    executable = Path(directory) / "test.exe"
    subprocess.run([
        os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-DCONFIG_BEPIO_CONTACTS_CAPACITY=4", "-DBEPIO_CONTACTS_STORAGE=1",
        "-I", str(ROOT), str(HERE / "main.c"), str(ROOT / "apps/contacts/model.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
