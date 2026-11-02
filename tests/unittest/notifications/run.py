"""Exercise the real notification store and controller with host mutex mocks."""
import os
from pathlib import Path
import subprocess
import tempfile
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
with tempfile.TemporaryDirectory(prefix="bepio-notifications-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    subprocess.run([os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT), "-I", str(ROOT / "tests/unittest/app_manager/host"),
                    str(HERE / "main.c"), str(ROOT / "apps/notifications/model.c"),
                    str(ROOT / "apps/notifications/controller.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
