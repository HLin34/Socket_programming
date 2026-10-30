#!/usr/bin/env python3
"""Integration test: graceful shutdown with four idle TCP clients."""
import argparse
import pathlib
import signal
import socket
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("--server", required=True)
args = parser.parse_args()
server = pathlib.Path(args.server).resolve()
project_root = server.parent.parent if server.parent.name == "build" else server.parent
process = subprocess.Popen([str(server)], cwd=project_root,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
clients = []
try:
    time.sleep(0.25)
    for _ in range(4):
        client = socket.create_connection(("127.0.0.1", 1234), timeout=3)
        client.settimeout(3)
        clients.append(client)
        header = bytearray()
        while len(header) < 4:
            data = client.recv(4 - len(header))
            if not data:
                raise AssertionError("Connection closed before menu frame")
            header.extend(data)
    process.send_signal(signal.SIGINT)
    stdout, stderr = process.communicate(timeout=5)
    assert process.returncode == 0, (process.returncode, stdout, stderr)
    assert "Graceful shutdown complete" in stdout, stdout
    for client in clients:
        # Buffered menu bytes may remain after shutdown; drain to EOF.
        while client.recv(4096):
            pass
    print("PASS: SIGINT closes four idle clients; worker threads joined; exit code 0")
finally:
    for client in clients:
        client.close()
    if process.poll() is None:
        process.kill()
        process.communicate()
