"""Check the release gate fails on this development firmware and validates layout."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("production_gate", ROOT / "tools/check_production.py")
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

current = gate.config_values(ROOT / "sdkconfig")
issues = gate.check(current, gate.partitions(ROOT / "partitions.csv"))
assert any("secure boot" in issue for issue in issues)
assert any("ota_0" in issue for issue in issues)
assert any("MESSAGES_DEMO" in issue for issue in issues)

layout = gate.partitions(ROOT / "partitions.production.csv")
expected = {key: "y" for key in (
    "CONFIG_BEPIO_PRODUCTION_BUILD", "CONFIG_SECURE_BOOT",
    "CONFIG_SECURE_FLASH_ENC_ENABLED", "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE",
    "CONFIG_NVS_ENCRYPTION", "CONFIG_BEPIO_OTA_UPDATES",
    "CONFIG_BEPIO_MESSAGING_TRANSPORT", "CONFIG_BEPIO_ENCRYPTED_MESSAGE_STORAGE",
)}
assert gate.check(expected, layout, 3 * 1024 * 1024) == []
layout["ota_1"] = ("app", "ota_1", layout["ota_0"][2], layout["ota_1"][3], "")
assert any("overlap" in issue for issue in gate.check(expected, layout))
layout = gate.partitions(ROOT / "partitions.production.csv")
layout["nvs_keys"] = (*layout["nvs_keys"][:4], "")
assert any("nvs_keys" in issue for issue in gate.check(expected, layout))
print("PASS: release gate rejects development firmware and overlapping OTA slots")
