"""Exercise the bench fixture over real local sockets (no controller required)."""
import http.client
import importlib.util
import json
from pathlib import Path
import socket
import threading
import time
import unittest

spec = importlib.util.spec_from_file_location("mock_arena", Path(__file__).resolve().parents[1] / "tools/fms_bench/mock_arena.py")
mock = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mock)
timing_spec = importlib.util.spec_from_file_location("check_timing", Path(__file__).resolve().parents[1] / "tools/fms_bench/check_timing.py")
timing = importlib.util.module_from_spec(timing_spec)
timing_spec.loader.exec_module(timing)


class BenchTests(unittest.TestCase):
    def setUp(self):
        self.server = mock.BenchServer(("127.0.0.1", 0))
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def request(self, path, data=None):
        connection = http.client.HTTPConnection(*self.server.server_address, timeout=3)
        try:
            connection.request("POST" if data is not None else "GET", path, json.dumps(data) if data is not None else None)
            response = connection.getresponse()
            return response.status, response.read().decode()
        finally:
            connection.close()

    def test_failure_retry_and_mapping(self):
        self.request("/control", dict(fail_stops=1))
        payload = [dict(channel=0, state=False)]
        self.assertEqual(self.request("/api/freezy/eStopState", payload)[0], 503)
        self.assertEqual(self.request("/api/freezy/eStopState", payload), (200, "eStop state updated successfully."))
        self.assertEqual(self.request("/api/freezy/eStopState", [dict(channel=1, state=True)])[0], 400)
        self.assertEqual([event["applied"] for event in self.server.events], [False, True])

    def test_lost_ack_is_applied(self):
        self.request("/control", dict(drop_stop_responses=1))
        with self.assertRaises(http.client.RemoteDisconnected):
            self.request("/api/freezy/eStopState", [dict(channel=0, state=False)])
        self.assertTrue(self.server.events[0]["applied"])
        self.assertTrue(self.server.events[0]["dropped_response"])

    def test_stack_defaults_and_start_response(self):
        status, body = self.request("/api/freezy/field_stack_light")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body), dict(redStackLight=True, blueStackLight=False,
                                              orangeStackLight=False, greenStackLight=False))
        self.assertEqual(self.request("/api/freezy/startMatch", dict(match="start")),
                         (200, "Field stack light state updated successfully."))

    def test_notifications_continue_during_delayed_http(self):
        self.request("/control", dict(delay_ms=1016, notifications_hz=100))
        with socket.create_connection(self.server.server_address, timeout=2) as ws:
            ws.sendall(b"GET /api/plc/websocket HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n")
            handshake = ws.recv(4096)
            self.assertIn(b"101", handshake)
            request = threading.Thread(target=lambda: self.request("/api/freezy/eStopState", [dict(channel=0, state=False)]))
            request.start()
            received = b""
            until = time.monotonic() + .25
            while time.monotonic() < until:
                received += ws.recv(8192)
            self.assertTrue(request.is_alive())
            self.assertGreater(received.count(b"plcIoChange"), 5)
            self.assertIn(b"arenaStatus", received)
            request.join(timeout=3)
            self.assertFalse(request.is_alive())


class TimingTests(unittest.TestCase):
    def test_healthy_and_stalled_capture(self):
        healthy = "[FMS TIMING] sample_gap_us=2000 unretained=0 FAULT=0\n[WS TIMING] gap_ms=30"
        self.assertEqual(timing.check(healthy), [])
        self.assertTrue(timing.check(healthy.replace("2000", "1154000")))
        self.assertTrue(timing.check(healthy.replace("gap_ms=30", "gap_ms=1016")))
        self.assertTrue(timing.check(healthy.replace("unretained=0", "unretained=1")))
        self.assertTrue(timing.check(healthy.replace("FAULT=0", "FAULT=1")))
        self.assertTrue(timing.check(""))


if __name__ == "__main__":
    unittest.main()
