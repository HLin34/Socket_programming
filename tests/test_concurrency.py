#!/usr/bin/env python3
"""Bounded TCP concurrency integration tests, standard library only."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import unittest

HOST = "127.0.0.1"
PORT = 1234
MAX_MESSAGE_SIZE = 4096


def read_exact(conn, n):
    data = bytearray()
    while len(data) < n:
        part = conn.recv(n - len(data))
        if not part:
            raise EOFError("connection closed during frame")
        data.extend(part)
    return bytes(data)


def read_frame(conn):
    size = struct.unpack("!I", read_exact(conn, 4))[0]
    if size > MAX_MESSAGE_SIZE:
        raise ValueError("response exceeded maximum frame size")
    return read_exact(conn, size).decode("utf-8")


def write_frame(conn, message):
    payload = message.encode("utf-8")
    conn.sendall(struct.pack("!I", len(payload)) + payload)


def connect_and_greet():
    conn = socket.create_connection((HOST, PORT), timeout=2)
    conn.settimeout(2)
    try:
        greeting = read_frame(conn)
        return conn, greeting
    except BaseException:
        conn.close()
        raise


class ConcurrencyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        sample = Path(cls.temp.name) / "students_sample.txt"
        sample.write_text("DEMO001 alex@example.com\n", encoding="utf-8")
        cls.server = subprocess.Popen(
            [ARGS.server, str(sample)], cwd=cls.temp.name,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if cls.server.poll() is not None:
                raise RuntimeError("test server quit before readiness")
            try:
                probe, greeting = connect_and_greet()
                probe.close()
                if "1.DNS" not in greeting:
                    raise RuntimeError("unexpected service on port 1234")
                time.sleep(.08)  # allow the worker to release readiness probe
                return
            except (OSError, EOFError):
                time.sleep(.05)
        cls.server.terminate()
        cls.server.wait(timeout=3)
        raise RuntimeError("test server was not ready; ensure port 1234 is free")

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, "server"):
            cls.server.terminate()
            try:
                cls.server.wait(timeout=2)
            except subprocess.TimeoutExpired:
                cls.server.kill()
                cls.server.wait(timeout=2)
        if hasattr(cls, "temp"):
            cls.temp.cleanup()

    def setUp(self):
        self.connections = []

    def tearDown(self):
        for conn in self.connections:
            conn.close()
        # The current implementation reclaims finished slots on next accept.
        time.sleep(.08)

    def open_client(self):
        conn, greeting = connect_and_greet()
        self.connections.append(conn)
        self.assertIn("1.DNS", greeting)
        return conn

    def four_clients(self):
        return [self.open_client() for _ in range(4)]

    def test_four_idle_clients_and_queries(self):
        clients = self.four_clients()
        for conn in clients:
            write_frame(conn, "2")
        for conn in clients:
            self.assertIn("Input student ID", read_frame(conn))
        for conn in clients:
            write_frame(conn, "DEMO001")
        for conn in clients:
            self.assertIn("alex@example.com", read_frame(conn))
        for conn in clients:
            self.assertIn("1.DNS", read_frame(conn))

    def test_fifth_connection_rejected(self):
        self.four_clients()
        with socket.create_connection((HOST, PORT), timeout=2) as extra:
            extra.settimeout(2)
            self.assertIn("Server busy", read_frame(extra))

    def test_ten_simultaneous_attempts_obey_capacity(self):
        self.four_clients()  # keep all four worker slots occupied
        def attempt(_):
            with socket.create_connection((HOST, PORT), timeout=2) as conn:
                conn.settimeout(2)
                return read_frame(conn)
        with ThreadPoolExecutor(max_workers=6) as executor:
            results = list(executor.map(attempt, range(6)))
        self.assertEqual(len(results), 6)
        self.assertTrue(all("Server busy" in message for message in results))

    def test_capacity_recovered_after_quit(self):
        clients = self.four_clients()
        write_frame(clients[0], "3")
        clients[0].close()
        self.connections.remove(clients[0])
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            conn, message = connect_and_greet()
            if "1.DNS" in message:
                self.connections.append(conn)
                write_frame(conn, "2")
                self.assertIn("Input student ID", read_frame(conn))
                write_frame(conn, "DEMO001")
                self.assertIn("alex@example.com", read_frame(conn))
                return
            conn.close()
            time.sleep(.05)
        self.fail("server did not recover released capacity")

    def test_abrupt_disconnect_recovers_capacity(self):
        clients = self.four_clients()
        clients[0].close()  # no QUIT frame
        self.connections.remove(clients[0])
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            conn, message = connect_and_greet()
            if "1.DNS" in message:
                self.connections.append(conn)
                self.assertIsNone(self.server.poll(), "server exited unexpectedly")
                return
            conn.close()
            time.sleep(.05)
        self.fail("capacity not recovered following abrupt disconnect")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", type=str, required=True)
    ARGS, remaining = parser.parse_known_args()
    ARGS.server = str(Path(ARGS.server).resolve())
    unittest.main(argv=[__file__, *remaining], verbosity=2)
