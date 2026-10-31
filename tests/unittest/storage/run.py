from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
here = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as tmp:
    exe = Path(tmp) / 'storage.exe'
    subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I'+str(here/'host'),
                    '-I'+str(root/'system/platform'), str(here/'main.c'),
                    str(root/'system/platform/bepio_storage_info.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
