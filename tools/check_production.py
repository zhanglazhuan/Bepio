"""Fail closed on known production blockers; run before signing or flashing."""
import argparse
from pathlib import Path
import sys


def config_values(path: Path) -> dict[str, str]:
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value.strip('"')
    return values


def size_bytes(value: str) -> int:
    value = value.strip().upper()
    if value.endswith("M"):
        return int(value[:-1], 0) * 1024 * 1024
    if value.endswith("K"):
        return int(value[:-1], 0) * 1024
    return int(value, 0)


def partitions(path: Path) -> dict[str, tuple[str, str, int, int, str]]:
    rows = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        name, kind, subtype, offset, size, *flags = [part.strip() for part in line.split(",")]
        rows[name] = kind, subtype, size_bytes(offset), size_bytes(size), flags[0] if flags else ""
    return rows


def check(config: dict[str, str], table: dict[str, tuple[str, str, int, int, str]],
          image_size: int | None = None) -> list[str]:
    problems = []
    required = {
        "CONFIG_BEPIO_PRODUCTION_BUILD": "production profile",
        "CONFIG_SECURE_BOOT": "signed secure boot",
        "CONFIG_SECURE_FLASH_ENC_ENABLED": "Flash encryption for firmware and NVS keys",
        "CONFIG_NVS_ENCRYPTION": "NVS encryption for Wi-Fi credentials",
        "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE": "bootloader OTA rollback",
        "CONFIG_BEPIO_OTA_UPDATES": "verified OTA update transport (not implemented)",
        "CONFIG_BEPIO_MESSAGING_TRANSPORT": "authenticated MN316 message transport (not implemented)",
        "CONFIG_BEPIO_ENCRYPTED_MESSAGE_STORAGE": "encrypted message storage replacing plaintext SPIFFS (not implemented)",
    }
    for key, label in required.items():
        if config.get(key) != "y":
            problems.append(f"missing {label}: {key}=y")
    for key in ("CONFIG_BEPIO_MESSAGES_DEMO", "CONFIG_BEPIO_CONSOLE"):
        if config.get(key) == "y":
            problems.append(f"disable {key} for release")
    expected = {
        "otadata": ("data", "ota"),
        "ota_0": ("app", "ota_0"),
        "ota_1": ("app", "ota_1"),
        "voice_data": ("data", "fat"),
        "storage": ("data", "spiffs"),
        "nvs_keys": ("data", "nvs_keys"),
    }
    for name, types in expected.items():
        if name not in table or table[name][:2] != types:
            problems.append(f"missing {name} partition of type {types[0]}/{types[1]}")
    if "nvs_keys" in table and "encrypted" not in table["nvs_keys"][4].split(":"):
        problems.append("nvs_keys partition must have encrypted flag")
    if image_size is not None:
        for name in ("ota_0", "ota_1"):
            if name in table and table[name][3] < image_size:
                problems.append(f"{name} is smaller than the firmware image")
    intervals = sorted((row[2], row[2] + row[3], name) for name, row in table.items())
    for (start, end, name), (next_start, _, next_name) in zip(intervals, intervals[1:]):
        if end > next_start:
            problems.append(f"partitions overlap: {name} and {next_name}")
    if intervals and intervals[-1][1] > 16 * 1024 * 1024:
        problems.append("partition layout exceeds 16 MiB Flash")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=Path("sdkconfig"))
    parser.add_argument("--partitions", type=Path, default=Path("partitions.csv"))
    parser.add_argument("--image", type=Path)
    args = parser.parse_args()
    config = config_values(args.config)
    table = partitions(args.partitions)
    issues = check(config, table, args.image.stat().st_size if args.image else None)
    if issues:
        print("BLOCKED: production release checks failed:")
        for issue in issues:
            print(f"- {issue}")
        return 1
    print("PASS: static production release checks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
