"""Compile actual Contacts MVC/shell and manager sources for host tests."""
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
with tempfile.TemporaryDirectory(prefix="bepio-contacts-tests-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    sources = [HERE / "main.c", ROOT / "system/appmgr/bepio_app_manager.c"]
    for app in ("settings", "contacts"):
        names = ["app.c", "model.c", "view.c", "controller.c"]
        if app == "contacts":
            names.append("shell.c")
        sources += [(ROOT / "tests/unittest/contacts/view_mock.c"
                     if app == "contacts" and name == "view.c"
                     else ROOT / "tests/unittest/app_manager/settings_view_mock.c"
                     if app == "settings" and name == "view.c"
                     else ROOT / "apps" / app / name) for name in names]
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
                    "-DCONFIG_BEPIO_MAX_APPS=8", "-DCONFIG_BEPIO_CONTACTS_CAPACITY=4",
                    "-I", str(HERE / "host"),
                    "-I", str(HERE.parent / "app_manager/host"),
                    "-I", str(ROOT),
                    "-I", str(ROOT / "system/appmgr"),
                    "-I", str(ROOT / "system/services/wifi"),
                    "-I", str(ROOT / "system/services/messaging"), *map(str, sources),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
