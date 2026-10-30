# TCP Client–Server Application (C++17)

[![C++ Build and Test](https://github.com/HLin34/Socket_programming/actions/workflows/ci.yml/badge.svg)](https://github.com/HLin34/Socket_programming/actions/workflows/ci.yml)

A C++17 TCP networking application originally developed for a **2023 Computer Networks course**, then improved in 2026 through three iterations: reliable TCP messaging, maintainable C++ architecture, and bounded concurrency with performance measurement.

**Scope:** Educational IPv4 loopback service (`127.0.0.1:1234`), **not** a production service, exchange gateway, or HFT latency claim.

## Highlights

- **Reliable framing:** four-byte network-byte-order message length, complete read/write loops, 4,096-byte payload limit.
- **Resource safety:** move-only RAII socket owner and automatic file-descriptor cleanup.
- **Modularity:** separate DNS resolution and student-directory components.
- **Bounded concurrency:** up to four active client handlers; excess connections receive a busy response.
- **Controlled shutdown:** SIGINT/SIGTERM handling, socket shutdown and worker joins for clean termination.
- **Thread-safe structured logging:** JSON Lines events with UTC timestamps and client IDs; request values and student emails are not logged.
- **Reproducible tooling:** CMake, Python standard-library integration tests, CTest, GitHub Actions CI, and a localhost benchmark tool.

## Architecture

```text
Interactive C++ Client(s)
        │   TCP / IPv4 loopback
        │   [4-byte length][payload]
        ▼
  C++ TCP Listener (server.cpp)
        │
        ├── bounded client handlers (max 4)
        │       ├── DNS resolver (dns.*)
        │       └── StudentDirectory (student_directory.*)
        │                      └── students_sample.txt (synthetic)
        ├── RAII socket ownership (socket_raii.hpp)
        ├── message framing (protocol.hpp)
        └── synchronized JSON event logging (logger.hpp)
```

The server handles a bounded number of clients concurrently. Requests on each individual client connection remain sequential; DNS resolution is synchronous. The service binds only to loopback.

## Repository guide

| Path | Responsibility |
| --- | --- |
| `server.cpp` / `client.cpp` | TCP listener, handlers, interactive client |
| `protocol.hpp` | Framed TCP messaging, partial I/O handling |
| `socket_raii.hpp` | Move-only socket resource owner |
| `dns.hpp`, `dns.cpp` | IPv4 hostname resolution |
| `student_directory.hpp`, `student_directory.cpp` | Read-only sample lookup |
| `logger.hpp` | Mutex-protected JSON Lines event logging |
| `tests/test_integration.py` | 9 functional/protocol test cases |
| `tests/test_concurrency.py` | 5 bounded-concurrency cases |
| `tests/test_shutdown.py` | SIGINT shutdown smoke/integration test |
| `benchmarks/benchmark.py` | Localhost latency and throughput measurements |
| `.github/workflows/ci.yml` | Ubuntu CI build and CTest |
| `docs/TECHNICAL_REPORT.md` | Extended engineering explanation |

## Requirements and build

- Linux or WSL 2 Ubuntu, with POSIX sockets and a C++17 compiler
- CMake >= 3.16, Python 3 (standard library for tests/benchmark)
- C++ threads supported by the toolchain

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Run** in two terminals, starting from the repository root so the sample file resolves correctly:

```bash
# Terminal 1
./build/server

# Terminal 2
./build/client
```

Client menu: `1` DNS (`localhost`), `2` student lookup (`DEMO001`), `3` quit. The sample record resolves to `alex@example.com` and is synthetic. An explicit student-data file path may be passed as the server's first argument.

To write local server events to a file:

```bash
./build/server 2> server.log
```

`server.log` and generated benchmark outputs should not be committed.

## Automated tests and CI

Stop any manually running server on port 1234, then:

```bash
ctest --test-dir build --output-on-failure
```

The configured CTest entries cover **9 functional/protocol cases + 5 concurrency cases + 1 shutdown check** (three CTest entries). Tests exercise lookup behavior, malformed input, reconnection, message fragmentation, capacity limits, abrupt disconnects and SIGINT shutdown. The GitHub Actions workflow builds and runs CTest on Ubuntu; the Phase 3 branch's initial workflow run passed. These tests do **not** establish complete race freedom or production readiness.

## Performance benchmark (WSL localhost)

```bash
python3 benchmarks/benchmark.py \
  --server ./build/server \
  --iterations 200 --warmup 30 --runs 3
```

The script starts its own server. Do not start another instance on port 1234. Each client performs sequential closed-loop requests on an established socket; 1, 2 or 4 clients are run concurrently. Each client/run has 30 warm-up and 200 measured synthetic student lookups; each concurrency level repeats three times.

| Concurrent clients | Median throughput (requests/s) | Median P50 (ms) | Median P95 (ms) | Median P99 (ms) |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 11.31 | 88.0039 | 91.9753 | 92.1352 |
| 2 | 22.64 | 88.0234 | 91.9142 | 92.1839 |
| 4 | 45.27 | 88.0082 | 91.8354 | 92.2020 |

*Observed October 30, 2026 on ASUS Zenbook S14 under WSL Ubuntu. Values are medians of per-run metrics; not pooled percentiles. Throughput is completed requests divided by elapsed test interval. Each timed request includes the application menu/prompt exchange, but excludes connection setup. The result includes Python client overhead, OS scheduling, WSL and server logging. It does not measure external networking, DNS resolution or exchange/HFT performance.*

Within this limited load range, throughput approximately scaled with the number of active clients while median per-request latency remained near 88 ms. The cause of that baseline latency has **not** been isolated; TCP delayed ACK/Nagle interaction is only a hypothesis, not a proven cause.

## Engineering evolution

| Phase | Engineering focus |
| --- | --- |
| **1 — Reliability** | Framed TCP messages, partial I/O, bounded payloads, safe input, synthetic data |
| **2 — Modern C++** | RAII socket ownership, modular DNS/data lookup, CMake, automated integration tests |
| **3 — Concurrency & observability** | Four-client bound, shutdown/worker cleanup, structured logging, load tests, benchmark, CI |

## Limitations and future experiments

- IPv4 loopback only; no authentication, TLS, persistence or network exposure hardening.
- Fixed maximum of four active clients and thread-per-client design; not an event-driven high-scale server.
- Synchronous `getaddrinfo()` may extend shutdown time if DNS resolution is in progress.
- Benchmark is a closed-loop localhost test, not an isolated wire RTT or production-grade microbenchmark.
- Potential future experiment: controlled `TCP_NODELAY` A/B measurement, **not** implemented or claimed as an optimization here.

See [Technical Report](docs/TECHNICAL_REPORT.md) for design and validation details.

## License

No open-source license is provided at present; reuse rights are not granted by default.
