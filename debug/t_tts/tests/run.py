"""Exercise the actual TTS service with the opaque ESP-TTS/Flash APIs mocked."""
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
headers = {
    "esp_err.h": """#pragma once
typedef int esp_err_t;
enum { ESP_OK=0, ESP_FAIL=-1, ESP_ERR_INVALID_STATE=1, ESP_ERR_INVALID_ARG,
 ESP_ERR_NOT_FOUND, ESP_ERR_NO_MEM, ESP_ERR_INVALID_CRC, ESP_ERR_NOT_SUPPORTED,
 ESP_ERR_NOT_FINISHED, ESP_ERR_TIMEOUT };
""",
    "esp_log.h": """#pragma once
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
""",
    "esp_partition.h": """#pragma once
#include <stddef.h>
#include "esp_err.h"
typedef int esp_partition_mmap_handle_t;
typedef struct { size_t size; } esp_partition_t;
enum { ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, ESP_PARTITION_MMAP_DATA };
const esp_partition_t *esp_partition_find_first(int, int, const char *);
esp_err_t esp_partition_mmap(const esp_partition_t *, size_t, size_t, int,
 const void **, esp_partition_mmap_handle_t *);
void esp_partition_munmap(esp_partition_mmap_handle_t);
""",
    "esp_timer.h": "#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n",
    "esp_tts.h": """#pragma once
typedef struct { int unused; } esp_tts_voice_t;
typedef void *esp_tts_handle_t;
esp_tts_voice_t *esp_tts_voice_set_init(const esp_tts_voice_t *, void *);
void esp_tts_voice_set_free(esp_tts_voice_t *);
esp_tts_handle_t esp_tts_create(esp_tts_voice_t *);
void esp_tts_destroy(esp_tts_handle_t);
int esp_tts_parse_chinese(esp_tts_handle_t, const char *);
short *esp_tts_stream_play(esp_tts_handle_t, int *, unsigned);
void esp_tts_stream_reset(esp_tts_handle_t);
""",
    "esp_tts_voice_template.h": '#pragma once\n#include "esp_tts.h"\nstatic const esp_tts_voice_t esp_tts_voice_template = {0};\n',
    "mbedtls/sha256.h": "#pragma once\n#include <stddef.h>\nint mbedtls_sha256(const unsigned char *, size_t, unsigned char[32], int);\n",
    "voice_model.h": "#pragma once\n#include <stdint.h>\n#define BEPIO_TTS_VOICE_BYTES 16\nstatic const uint8_t bepio_tts_voice_sha256[32] = {0};\n",
}
with tempfile.TemporaryDirectory(prefix="bepio-tts-tests-") as directory:
    host = Path(directory)
    for name, text in headers.items():
        header = host / name
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text(text, encoding="utf-8")
    service = ROOT / "system/services/tts"
    executable = host / ("test.exe" if os.name == "nt" else "test")
    subprocess.run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
        "-I", str(host), "-I", str(service), str(HERE / "main.c"),
        str(service / "bepio_tts.c"), str(service / "tts_text.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
