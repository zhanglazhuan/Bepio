"""Compile real app-manager/Settings sources for single-threaded host tests."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
compiler = os.environ.get("CC", "gcc")
if not shutil.which(compiler):
    raise SystemExit(f"C compiler not found: {compiler}; set CC to its executable path")
with tempfile.TemporaryDirectory(prefix="bepio-app-tests-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    sources = [HERE / "main.c", ROOT / "system/appmgr/bepio_app_manager.c"]
    sources += [(HERE / "settings_view_mock.c" if name == "view.c" else ROOT / "apps/settings" / name) for name in
                ("app.c", "model.c", "view.c", "controller.c")]
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-DCONFIG_BEPIO_MAX_APPS=8", "-I", str(HERE / "host"),
                    "-I", str(ROOT),
                    "-I", str(ROOT / "system/appmgr"),
                    "-I", str(ROOT / "system/services/wifi"),
                    "-I", str(ROOT / "system/services/messaging"), *map(str, sources),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
