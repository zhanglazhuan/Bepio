"""Build actual messaging service, MVC, shell and contact integration on the host."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
compiler = os.environ.get("CC", "gcc")
if not shutil.which(compiler):
    raise SystemExit(f"C compiler not found: {compiler}")
with tempfile.TemporaryDirectory(prefix="bepio-messages-tests-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    sources = [HERE / "main.c", ROOT / "system/appmgr/bepio_app_manager.c",
               ROOT / "system/services/messaging/messaging.c"]
    for app in ("settings", "contacts", "messages"):
        names = ["app.c", "model.c", "view.c", "controller.c"]
        if app == "messages":
            names.append("shell.c")
        sources += [(ROOT / "tests/unittest/contacts/view_mock.c"
                     if app == "contacts" and name == "view.c"
                     else ROOT / "tests/unittest/app_manager/settings_view_mock.c"
                     if app == "settings" and name == "view.c"
                     else ROOT / "apps" / app / name) for name in names]
    defines = ["CONFIG_BEPIO_MAX_APPS=8", "CONFIG_BEPIO_APP_CONTACTS=1",
               "CONFIG_BEPIO_CONTACTS_CAPACITY=4", "CONFIG_BEPIO_MESSAGES_DEMO=1",
               "CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY=3", "CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY=3"]
    subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
                    "-include", str(HERE / "host/errno_compat.h"),
                    *["-D" + define for define in defines],
                    "-I", str(HERE.parent / "contacts/host"),
                    "-I", str(HERE.parent / "app_manager/host"),
                    "-I", str(ROOT),
                    "-I", str(ROOT / "system/appmgr"),
                    "-I", str(ROOT / "system/services/wifi"),
                    "-I", str(ROOT / "system/services/messaging"), *map(str, sources),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
