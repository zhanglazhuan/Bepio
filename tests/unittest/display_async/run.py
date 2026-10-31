"""Compile the actual LCD driver, replacing only ESP-IDF SDK/hardware entrypoints."""
from pathlib import Path
import os
import subprocess
import tempfile
here = Path(__file__).resolve().parent
root = here.parents[2]
with tempfile.TemporaryDirectory(prefix='bepio-lcd-async-') as directory:
    executable = Path(directory) / 'test.exe'
    subprocess.run([os.environ.get('CC', 'gcc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(here/'host'), '-I'+str(root/'boards/esp32s3'),
                    '-I'+str(root/'drivers/st7789'), str(here/'main.c'), str(here/'sdk_mock.c'),
                    str(root/'drivers/st7789/bepio_lcd.c'), '-o', str(executable)], check=True)
    for scenario in ('success', 'timeout', 'submit-error'):
        subprocess.run([str(executable), scenario], check=True)
