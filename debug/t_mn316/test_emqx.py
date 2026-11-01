"""Receive an actual MN316 uplink over verified TLS; requires pyserial."""
import argparse
import codecs
import json
from pathlib import Path
import re
import secrets
import socket
import ssl
import struct
import threading
import time

import serial

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CONFIG = (ROOT / "system/mn316_ctrl/bepio_mn316_config.h").read_text(encoding="utf-8")

def config_string(name):
    return json.loads(re.search(r"^#define\s+" + name + r'\s+(".*")', CONFIG, re.M)[1])

HOST = config_string("BEPIO_MN316_EMQX_HOST")
PORT = int(re.search(r"^#define\s+BEPIO_MN316_EMQX_PORT\s+(\d+)", CONFIG, re.M)[1])
PEER = config_string("BEPIO_MN316_TEST_PEER")
TOPIC = config_string("BEPIO_MN316_TOPIC_PREFIX") + PEER + "/inbox"


def field(text):
    data = text.encode()
    return struct.pack("!H", len(data)) + data


def send(sock, header, body):
    size, encoded = len(body), bytearray()
    while True:
        size, digit = divmod(size, 128)
        encoded.append(digit | (128 if size else 0))
        if not size:
            break
    sock.sendall(bytes([header]) + encoded + body)


def exact(sock, count):
    data = bytearray()
    while len(data) < count:
        part = sock.recv(count - len(data))
        if not part:
            raise ConnectionError("MQTT peer closed")
        data.extend(part)
    return bytes(data)


def receive(sock):
    header, size = exact(sock, 1)[0], 0
    for index in range(4):
        digit = exact(sock, 1)[0]
        size |= (digit & 127) << (index * 7)
        if not digit & 128:
            if size > 4096:
                raise ValueError("Oversized MQTT packet")
            return header, exact(sock, size)
    raise ValueError("Malformed MQTT packet")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--device-id", required=True, help="Numeric ID printed by the debug firmware")
    parser.add_argument("--text", default="MN316 TLS test message")
    args = parser.parse_args()
    if not args.device_id.isascii() or not args.device_id.isdecimal() or len(args.device_id) > 24:
        parser.error("--device-id must contain 1-24 ASCII digits")
    if any(c in args.text for c in "\r\n") or len(args.text.encode()) > 384:
        parser.error("Text must be a single line, at most 384 UTF-8 bytes")
    local = (ROOT / "system/mn316_ctrl/emqx_local.h").read_text(encoding="utf-8")
    username, password = [json.loads(re.search(name + r'\s+(".*")', local)[1])
                          for name in ("BEPIO_EMQX_USERNAME", "BEPIO_EMQX_PASSWORD")]
    host = HOST
    ca = ROOT / "system/mn316_ctrl/certs/emqx_ca.pem"
    context = ssl.create_default_context(cafile=str(ca))
    with socket.create_connection((host, PORT), timeout=15) as raw:
        with context.wrap_socket(raw, server_hostname=host) as peer:
            peer.settimeout(240)
            body = field("MQTT") + bytes([4, 0xc2, 0, 0])
            body += field(str(secrets.randbelow(10**15 - 10**14) + 10**14)) + field(username) + field(password)
            send(peer, 0x10, body)
            header, response = receive(peer)
            assert (header, response) == (0x20, b"\0\0"), f"MQTT login rejected: {header:#x}/{response.hex()}"
            send(peer, 0x82, b"\0\1" + field(TOPIC) + b"\0")
            assert receive(peer) == (0x90, b"\0\1\0"), "Subscription rejected"
            print(f"PC subscribed {TOPIC}; TLS certificate verified", flush=True)
            outcome = {}

            def read_peer():
                try:
                    header, body = receive(peer)
                    assert header == 0x30, f"Unexpected MQTT packet {header:#x}"
                    size = int.from_bytes(body[:2], "big")
                    assert body[2:2 + size].decode() == TOPIC
                    message = json.loads(body[2 + size:])
                    assert message["From"] == args.device_id and message["To"] == PEER
                    assert message["Text"] == args.text
                    outcome["message"] = message
                    print("PC_RECEIVE_PASS " + json.dumps(message, ensure_ascii=False), flush=True)
                except Exception as error:
                    outcome["error"] = str(error)

            threading.Thread(target=read_peer, daemon=True).start()
            port = serial.Serial(port=None, baudrate=115200, timeout=0.1)
            port.dtr = port.rts = False
            port.port = args.port
            port.open()
            try:
                log_path = HERE / "build/emqx-hardware.log"
                with log_path.open("w", encoding="utf-8") as log:
                    decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
                    transcript = ""

                    def read_for(seconds, marker=None):
                        nonlocal transcript
                        deadline = time.monotonic() + seconds
                        while time.monotonic() < deadline:
                            data = port.read(port.in_waiting or 1)
                            if data:
                                if b"\x1b[6n" in data:
                                    port.write(b"\x1b[1;1R")
                                text = decoder.decode(data)
                                print(text, end="", flush=True)
                                log.write(text)
                                log.flush()
                                transcript += text
                                if marker and marker in transcript:
                                    return

                    port.write(b"\r")
                    read_for(5)
                    port.write(("emqx_send 30 " + json.dumps(args.text, ensure_ascii=False) + "\r").encode())
                    read_for(300, "EMQX_SEND_DONE result=")
                    read_for(1)
                    if "EMQX_SEND_DONE result=0" not in transcript:
                        raise RuntimeError("Device session failed or timed out; inspect build/emqx-hardware.log")
                    if "message" not in outcome:
                        raise RuntimeError("PC did not receive uplink: " + outcome.get("error", "no message"))
                    log.write("\nPC_RECEIVE_PASS " + json.dumps(outcome["message"], ensure_ascii=False) + "\n")
                    print("PASS: actual MN316 -> EMQX -> PC message; session cleanup completed", flush=True)
            finally:
                port.close()


if __name__ == "__main__":
    main()
