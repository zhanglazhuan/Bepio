"""Bounded authentication-only probes; never log credentials or publish messages."""
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import secrets
import socket
import ssl
import time

from test_emqx import field, receive, send, HOST, PORT

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
REASONS = {0: "accepted", 5: "not_authorized (MQTT 3.1.1)",
           0x85: "client_identifier_not_valid", 0x86: "bad_username_or_password",
           0x87: "not_authorized", 0x8a: "banned", 0x9f: "connection_rate_exceeded"}


def read_credentials(path):
    text = path.read_text(encoding="utf-8")
    return tuple(json.loads(re.search(r"^\s*#define\s+" + name + r'\s+(".*")', text, re.M)[1])
                 for name in ("BEPIO_EMQX_USERNAME", "BEPIO_EMQX_PASSWORD"))


def probe(version, client_id, tls12, credentials):
    result = {"utc": datetime.now(timezone.utc).isoformat(),
              "protocol": version, "client_id": client_id, "keepalive": 60}
    context = ssl.create_default_context(cafile=str(ROOT / "system/mn316_ctrl/certs/emqx_ca.pem"))
    if tls12:
        context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
    try:
        with socket.create_connection((HOST, PORT), timeout=15) as raw:
            with context.wrap_socket(raw, server_hostname=HOST) as peer:
                peer.settimeout(15)
                body = field("MQTT") + bytes([version, 0xc2, 0, 60])
                if version == 5:
                    body += b"\0"  # CONNECT properties length
                body += field(client_id) + field(credentials[0]) + field(credentials[1])
                result["tls"] = peer.version()
                result["connect_body_bytes"] = len(body)
                send(peer, 0x10, body)
                header, response = receive(peer)
                result["response_type"] = hex(header)
                if header == 0x20 and len(response) >= 2:
                    code = response[1]
                    result["reason_code"] = hex(code)
                    result["reason"] = REASONS.get(code, "other")
                    if code == 0:
                        send(peer, 0xe0, b"")
                else:
                    result["error"] = "Expected CONNACK"
    except Exception as error:
        result["error"] = f"{type(error).__name__}: {error}"
    return result


def main():
    credentials = read_credentials(ROOT / "system/mn316_ctrl/emqx_local.h")
    firmware = (HERE / "build/t_mn316.bin").read_bytes()
    report = {"host": HOST, "port": PORT,
              "credential_source": "system/mn316_ctrl/emqx_local.h",
              "credential_lengths": [len(value) for value in credentials],
              "credentials_match_firmware": all(value.encode() + b"\0" in firmware for value in credentials),
              "expected_client_id_in_firmware": b"Pager_MN316\0" in firmware,
              "probes": []}
    random_id = "bepio-auth-" + secrets.token_hex(4)
    for version, client_id, tls12 in [(4, "Pager_MN316", True), (4, random_id, False),
                                      (5, "Pager_MN316", True), (5, random_id, False)]:
        result = probe(version, client_id, tls12, credentials)
        report["probes"].append(result)
        print(json.dumps(result, ensure_ascii=False), flush=True)
        time.sleep(1)
    path = HERE / "build/emqx-auth-diagnosis.json"
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Saved redacted diagnosis: {path}")


if __name__ == "__main__":
    main()
