"""Actual service with deterministic SDK mocks; does not prove RTOS scheduling or RF."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
here = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as tmp:
    exe = Path(tmp) / 'wifi.exe'
    subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-D_POSIX_C_SOURCE=200809L',
                    '-I'+str(here/'host'), '-I'+str(root/'system/services/wifi'),
                    '-I'+str(root/'system/platform'), str(here/'main.c'),
                    str(root/'system/services/wifi/bepio_wifi.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
