#!/usr/bin/env python3
"""Closed-loop TCP application request/response benchmark (standard library only)."""
import argparse
import csv
import json
import math
import platform
import socket
import statistics
import struct
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path

HOST = '127.0.0.1'
PORT = 1234
MAX_FRAME = 4096


def read_exact(sock, size):
    result = bytearray()
    while len(result) < size:
        chunk = sock.recv(size - len(result))
        if not chunk:
            raise ConnectionError('Unexpected end of TCP stream')
        result.extend(chunk)
    return bytes(result)


def recv_message(sock):
    length = struct.unpack('!I', read_exact(sock, 4))[0]
    if length > MAX_FRAME:
        raise ValueError(f'Frame exceeds {MAX_FRAME} bytes')
    return read_exact(sock, length).decode('utf-8')


def send_message(sock, message):
    payload = message.encode('utf-8')
    if len(payload) > MAX_FRAME:
        raise ValueError('Oversized message')
    sock.sendall(struct.pack('!I', len(payload)) + payload)


def percent(values, p):
    x = sorted(values)
    pos = (len(x) - 1) * p / 100.0
    lo = int(pos)
    hi = min(lo + 1, len(x) - 1)
    return x[lo] + (x[hi] - x[lo]) * (pos - lo)


def worker(start_barrier, iterations, warmup):
    samples = []
    measured_start = None
    measured_end = None
    with socket.create_connection((HOST, PORT), timeout=5) as sock:
        sock.settimeout(5)
        menu = recv_message(sock)
        if '1.DNS' not in menu:
            raise RuntimeError(f'Unexpected initial menu: {menu!r}')
        start_barrier.wait(timeout=10)
        for i in range(warmup + iterations):
            # Time the already-connected TCP request-response exchange;
            # includes protocol frames and menu handshakes, excludes setup/teardown.
            start = time.perf_counter_ns()
            if i == warmup:
                measured_start = start
            send_message(sock, '2')
            prompt = recv_message(sock)
            if 'student ID' not in prompt:
                raise RuntimeError(f'Unexpected prompt: {prompt!r}')
            send_message(sock, 'DEMO001')
            answer = recv_message(sock)
            if 'alex@example.com' not in answer:
                raise RuntimeError(f'Unexpected answer: {answer!r}')
            menu = recv_message(sock)
            end = time.perf_counter_ns()
            if '1.DNS' not in menu:
                raise RuntimeError(f'Unexpected menu: {menu!r}')
            if i >= warmup:
                samples.append((end - start) / 1e6)
                measured_end = end
        send_message(sock, '3')
    return samples, measured_start, measured_end


def launch_server(executable, log_path):
    # Log redirection prevents a pipe from filling and slowing/freezing the server.
    log_handle = log_path.open('w', encoding='utf-8')
    project_dir = executable.parent if (executable.parent / 'students_sample.txt').exists() else executable.parent.parent
    process = subprocess.Popen([str(executable)], cwd=project_dir,
                               stdout=log_handle, stderr=subprocess.STDOUT)
    try:
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f'Server exited early ({process.returncode}); see {log_path}')
            try:
                with socket.create_connection((HOST, PORT), timeout=.2) as sock:
                    if '1.DNS' in recv_message(sock):
                        return process, log_handle
            except (OSError, EOFError, ConnectionError, TimeoutError):
                pass
            time.sleep(.05)
        raise TimeoutError(f'Server not ready; see {log_path}')
    except BaseException:
        process.terminate()
        try: process.wait(timeout=3)
        except subprocess.TimeoutExpired: process.kill(); process.wait()
        log_handle.close()
        raise


def run_case(server, count, iterations, warmup):
    if server.poll() is not None:
        raise RuntimeError('Server not running')
    barrier = threading.Barrier(count + 1)
    with ThreadPoolExecutor(max_workers=count) as executor:
        futures = [executor.submit(worker, barrier, iterations, warmup) for _ in range(count)]
        try:
            barrier.wait(timeout=10)
        except threading.BrokenBarrierError:
            # Surface the originating worker exception rather than hanging.
            for future in futures:
                if future.done(): future.result()
            raise
        samples = []
        starts, ends = [], []
        for future in futures:
            # Include warm-up requests and allow a margin for slow WSL/TCP round trips.
            result_samples, first, last = future.result(timeout=max(60, (iterations + warmup) * 0.30))
            samples.extend(result_samples)
            starts.append(first)
            ends.append(last)
        duration = (max(ends) - min(starts)) / 1e9
    expected = count * iterations
    if len(samples) != expected:
        raise RuntimeError(f'Expected {expected} results, got {len(samples)}')
    return {'clients': count, 'requests': expected,
            'elapsed_s': round(duration, 6),
            'rps': round(expected / duration, 2),
            'mean_ms': round(statistics.mean(samples), 4),
            'p50_ms': round(percent(samples, 50), 4),
            'p95_ms': round(percent(samples, 95), 4),
            'p99_ms': round(percent(samples, 99), 4),
            'max_ms': round(max(samples), 4)}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server', type=Path, default=Path('build/server'))
    p.add_argument('--iterations', type=int, default=200, help='Measured requests per client per run')
    p.add_argument('--warmup', type=int, default=30, help='Unmeasured requests per client')
    p.add_argument('--runs', type=int, default=3)
    p.add_argument('--clients', type=int, nargs='+', default=[1, 2, 4])
    p.add_argument('--output', type=Path, default=Path('benchmark_results'))
    a = p.parse_args()
    if a.iterations < 10 or a.warmup < 0 or a.runs < 1 or any(n < 1 or n > 4 for n in a.clients):
        p.error('Require iterations >= 10, warmup >= 0, runs >= 1, and clients between 1 and 4')
    executable = a.server.expanduser().resolve()
    if not executable.is_file(): p.error(f'Server executable not found: {executable}')
    # Avoid accidentally benchmark another already running process.
    try:
        with socket.create_connection((HOST, PORT), timeout=.3):
            p.error(f'Port {PORT} is already in use. Stop the existing server first.')
    except OSError:
        pass
    a.output.mkdir(parents=True, exist_ok=True)
    log_path = a.output / 'server_benchmark.log'
    server, handle = launch_server(executable, log_path)
    records = []
    error = None
    try:
        for clients in a.clients:
            for run in range(1, a.runs + 1):
                record = run_case(server, clients, a.iterations, a.warmup)
                record['run'] = run
                records.append(record)
                print(f'{clients} clients | run {run} | {record["rps"]:.2f} req/s | '
                      f'p50 {record["p50_ms"]:.3f} ms | p99 {record["p99_ms"]:.3f} ms', flush=True)
    finally:
        # SIGINT triggers the server's graceful shutdown path.
        import signal
        if server.poll() is None:
            server.send_signal(signal.SIGINT)
            try: server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill(); server.wait()
        handle.close()
    metadata = {'timestamp_utc': datetime.now(timezone.utc).isoformat(),
                'host': HOST, 'port': PORT, 'machine': platform.platform(),
                'python': sys.version.split()[0], 'server_executable': str(executable),
                'iterations_per_client': a.iterations, 'warmup_per_client': a.warmup,
                'runs_per_level': a.runs,
                'measurement': 'closed-loop request-response on established localhost TCP sockets; student lookup, including menu and prompt frames; excludes connection establishment',
                'warning': 'Benchmark includes Python client overhead, OS scheduling, server logging and WSL overhead. Not HFT/exchange latency.'}
    csv_path = a.output / 'results.csv'
    with csv_path.open('w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=['clients','run','requests','elapsed_s','rps','mean_ms','p50_ms','p95_ms','p99_ms','max_ms'])
        writer.writeheader(); writer.writerows(records)
    (a.output / 'results.json').write_text(json.dumps({'metadata':metadata,'runs':records},indent=2)+'\n',encoding='utf-8')
    lines = ['# TCP Application Benchmark', '', '> Localhost / WSL closed-loop test; not production trading latency.', '',
             f'- UTC run time: {metadata["timestamp_utc"]}',
             f'- Measured requests per client/run: {a.iterations}', f'- Warm-up per client/run: {a.warmup}',
             f'- Runs per concurrency level: {a.runs}', '',
             '| Clients | Median RPS across runs | Median P50 (ms) | Median P95 (ms) | Median P99 (ms) |',
             '|---:|---:|---:|---:|---:|']
    for clients in a.clients:
        cases = [r for r in records if r['clients'] == clients]
        median = lambda field: statistics.median(r[field] for r in cases)
        lines.append(f'| {clients} | {median("rps"):.2f} | {median("p50_ms"):.4f} | {median("p95_ms"):.4f} | {median("p99_ms"):.4f} |')
    lines += ['', '## Methodology', metadata['measurement'], '', 'Each client sends requests sequentially; clients run concurrently.',
              'Throughput uses measured completions / wall-clock duration; warm-up excluded from samples, but duration includes a small start/scheduling offset.',
              'Per-run percentiles are computed over measured request latencies; table reports the median of per-run percentiles.',
              'Each request exchanges option, prompt, student ID, response, and the next menu.', '',
              '## Limitations', metadata['warning'], 'Tests use a fixed synthetic student ID and localhost (127.0.0.1).',
              'No DNS latency, connection-establishment latency, external network or actual exchange behavior is measured.', '']
    (a.output / 'REPORT.md').write_text('\n'.join(lines),encoding='utf-8')
    print(f'Outputs: {csv_path}, {a.output / "results.json"}, {a.output / "REPORT.md"}')

if __name__ == '__main__':
    main()
