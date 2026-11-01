"""Exercise the actual MQTT session with deterministic transport and clock mocks."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
with tempfile.TemporaryDirectory(prefix="bepio-mqtt-") as directory:
    executable = Path(directory) / "mqtt_test.exe"
    subprocess.run([os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra",
                    "-Werror", "-I", str(ROOT / "system/mn316_ctrl"),
                    "-I", str(ROOT / "drivers/mn316"),
                    str(HERE / "main.c"),
                    str(ROOT / "system/mn316_ctrl/mn316_mqtt.c"),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    session = Path(directory) / "session_test.exe"
    cjson = ROOT / "managed_components/espressif__cjson/cJSON"
    subprocess.run([os.environ.get("CC", "gcc"), "-std=gnu11", "-Wall", "-Wextra",
                    "-Werror", "-include", str(HERE / "host/time_compat.h"),
                    "-I", str(HERE / "host"),
                    "-I", str(ROOT / "system/mn316_ctrl"), "-I", str(ROOT / "drivers/mn316"),
                    "-I", str(cjson), str(HERE / "session.c"),
                    str(ROOT / "system/mn316_ctrl/bepio_mn316_session.c"),
                    str(ROOT / "system/mn316_ctrl/mn316_mqtt.c"),
                    str(ROOT / "system/mn316_ctrl/pager_message.c"), str(cjson / "cJSON.c"),
                    "-o", str(session)], check=True)
    subprocess.run([str(session)], check=True)
