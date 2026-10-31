from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory() as tmp:
    exe = Path(tmp) / 'orientation.exe'
    subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I'+str(root/'system/uilv'), '-I'+str(root/'boards/esp32s3'), str(Path(__file__).with_name('main.c')), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
