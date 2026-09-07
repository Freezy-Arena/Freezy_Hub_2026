"""Isolated FMS_TABLE bench arena; Python standard library only. Never starts real matches."""
import argparse
import base64
import hashlib
import json
import socket
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class BenchServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, log_path=None):
        super().__init__(address, Handler)
        self.lock = threading.Lock()
        self.log_path = log_path
        self.events = []
        self.controls = dict(delay_ms=0, fail_stops=0, drop_stop_responses=0,
                             notifications_hz=50, redStackLight=True,
                             blueStackLight=False, orangeStackLight=False,
                             greenStackLight=False)

    def record(self, **event):
        event["time_ns"] = time.monotonic_ns()
        with self.lock:
            self.events.append(event)
            if self.log_path:
                with open(self.log_path, "a", encoding="utf-8") as out:
                    out.write(json.dumps(event) + "\n")


def frame(message):
    payload = json.dumps(message, separators=(",", ":")).encode()
    header = bytes([0x81, len(payload)]) if len(payload) < 126 else b"\x81\x7e" + struct.pack("!H", len(payload))
    return header + payload


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def reply(self, code, body):
        encoded = body.encode()
        try:
            self.send_response(code)
            self.send_header("Content-Length", str(len(encoded)))
            self.send_header("Content-Type", "application/json" if body.startswith("{") else "text/plain")
            self.end_headers()
            self.wfile.write(encoded)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_POST(self):
        try:
            data = json.loads(self.rfile.read(int(self.headers.get("Content-Length", "0"))))
        except (ValueError, json.JSONDecodeError):
            self.reply(400, "Invalid request payload")
            return
        if self.path == "/control":
            with self.server.lock:
                for key, value in data.items():
                    if key in self.server.controls:
                        self.server.controls[key] = value
            self.reply(200, "Control updated")
            return
        with self.server.lock:
            options = self.server.controls.copy()
            fail = self.path.endswith("eStopState") and options["fail_stops"] > 0
            drop = self.path.endswith("eStopState") and options["drop_stop_responses"] > 0
            if fail:
                self.server.controls["fail_stops"] -= 1
            elif drop:
                self.server.controls["drop_stop_responses"] -= 1
        if self.path == "/api/freezy/eStopState":
            if not isinstance(data, list) or any(x.get("channel") != 0 or type(x.get("state")) is not bool for x in data):
                self.reply(400, "Invalid table mapping")
                return
            code = 503 if fail else 200
            body = "Unavailable" if fail else "eStop state updated successfully."
        elif self.path == "/api/freezy/startMatch":
            code, body = 200, "Field stack light state updated successfully."
        else:
            self.reply(404, "Unknown endpoint")
            return
        # Model the actual handler: apply before writing the response. A lost
        # response is ambiguous; the client must retry the same stop head.
        self.server.record(path=self.path, data=data, status=code, applied=code == 200,
                           dropped_response=drop and not fail)
        time.sleep(max(0, options["delay_ms"]) / 1000)
        if drop and not fail:
            self.close_connection = True
            try:
                self.connection.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            return
        self.reply(code, body)

    def do_GET(self):
        if self.path == "/api/plc/websocket":
            self.websocket()
            return
        if self.path == "/events":
            with self.server.lock:
                events = list(self.server.events)
            self.reply(200, json.dumps(events))
            return
        if self.path != "/api/freezy/field_stack_light":
            self.reply(404, "Unknown endpoint")
            return
        with self.server.lock:
            options = self.server.controls.copy()
        self.server.record(path=self.path)
        time.sleep(max(0, options["delay_ms"]) / 1000)
        self.reply(200, json.dumps({key: options[key] for key in
            ("redStackLight", "blueStackLight", "orangeStackLight", "greenStackLight")}))

    def websocket(self):
        key = self.headers.get("Sec-WebSocket-Key", "")
        accept = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
        self.send_response(101)
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", accept)
        self.end_headers()
        self.server.record(path=self.path, connected=True)
        try:
            while True:
                with self.server.lock:
                    hz = self.server.controls["notifications_hz"]
                if hz > 0:
                    # arenaStatus is injected deliberately as a stress case;
                    # the inspected production PLC route only sends PLC/LED.
                    for message in (
                        dict(type="plcIoChange", data=dict(Coils=[False] * 32, Registers=[0] * 32)),
                        dict(type="arenaStatus", data=dict(MatchState=0)),
                        dict(type="setLedMode", data=dict(RedMode=1, BlueMode=2)),
                    ):
                        self.wfile.write(frame(message))
                    self.wfile.flush()
                time.sleep(1 / max(1, hz))
        except (OSError, ValueError):
            self.close_connection = True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--log", default="fms-bench.jsonl")
    args = parser.parse_args()
    server = BenchServer((args.bind, args.port), args.log)
    print(f"Bench arena on {args.bind}:{args.port}; recording {args.log}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        server.server_close()
