#!/usr/bin/env python3
"""End-to-end tests for the length-prefixed TCP server (stdlib only)."""
import argparse
import socket
import struct
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

HOST = '127.0.0.1'
PORT = 1234
MAX_MESSAGE_SIZE = 4096


def recv_exact(conn, count):
    chunks = bytearray()
    while len(chunks) < count:
        piece = conn.recv(count - len(chunks))
        if not piece:
            raise EOFError('socket closed before complete message')
        chunks.extend(piece)
    return bytes(chunks)


def recv_message(conn):
    size = struct.unpack('!I', recv_exact(conn, 4))[0]
    if size > MAX_MESSAGE_SIZE:
        raise ValueError('oversized response')
    return recv_exact(conn, size).decode('utf-8')


def send_message(conn, text, fragments=False):
    data = text.encode('utf-8')
    payload = struct.pack('!I', len(data)) + data
    if fragments:
        for byte in payload:
            conn.sendall(bytes([byte]))
    else:
        conn.sendall(payload)


class ServerIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        path = Path(cls.temp.name) / 'students_sample.txt'
        path.write_text('DEMO001 alex@example.com\nDEMO002 sam@example.com\n', encoding='utf-8')
        cls.process = subprocess.Popen([ARGS.server, str(path)], cwd=cls.temp.name,
                                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if cls.process.poll() is not None:
                raise RuntimeError('server exited before startup')
            try:
                with socket.create_connection((HOST, PORT), timeout=.3):
                    return
            except OSError:
                time.sleep(.05)
        cls.process.terminate()
        cls.process.wait(timeout=2)
        raise RuntimeError('server did not start on localhost:1234')

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, 'process'):
            cls.process.terminate()
            try:
                cls.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                cls.process.kill()
                cls.process.wait()
            cls.process.stderr.close()
        if hasattr(cls, 'temp'):
            cls.temp.cleanup()

    def connect(self):
        conn = socket.create_connection((HOST, PORT), timeout=3)
        conn.settimeout(3)
        self.assertIn('1.DNS', recv_message(conn))
        return conn

    def query(self, option, value, fragments=False):
        with self.connect() as conn:
            send_message(conn, option, fragments)
            self.assertIn('Input ', recv_message(conn))
            send_message(conn, value, fragments)
            return recv_message(conn)

    def test_dns_localhost(self):
        self.assertIn('127.0.0.1', self.query('1', 'localhost'))

    def test_dns_blank(self):
        self.assertIn('Invalid hostname', self.query('1', ''))

    def test_student_found(self):
        self.assertEqual('Email: alex@example.com\n', self.query('2', 'DEMO001'))

    def test_student_missing(self):
        self.assertIn('No such student ID', self.query('2', 'UNKNOWN'))

    def test_invalid_menu(self):
        with self.connect() as conn:
            send_message(conn, '9')
            self.assertIn('Invalid requirement', recv_message(conn))
            self.assertIn('1.DNS', recv_message(conn))
            send_message(conn, '3')

    def test_fragmented_frame(self):
        self.assertIn('alex@example.com', self.query('2', 'DEMO001', fragments=True))

    def test_disconnect_reconnect(self):
        with self.connect() as conn:
            send_message(conn, '3')
        self.assertIn('127.0.0.1', self.query('1', 'localhost'))

    def test_oversized_frame_recovery(self):
        with self.connect() as conn:
            conn.sendall(struct.pack('!I', MAX_MESSAGE_SIZE + 1))
        self.assertIn('alex@example.com', self.query('2', 'DEMO001'))

    def test_abrupt_disconnect_recovery(self):
        conn = self.connect()
        conn.close()
        self.assertIn('alex@example.com', self.query('2', 'DEMO001'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', required=True)
    ARGS = parser.parse_args()
    unittest.main(argv=['test_integration.py'], verbosity=2)
