# TCP Client–Server Application (C++17)

A small TCP networking application originally built for a 2023 Computer Networks course and subsequently refactored to practice reliable socket communication, modern C++ resource management, modular design, and automated integration testing.

> **Scope:** An educational, single-threaded IPv4 loopback service — not a production server or a high-frequency trading system.

## Features

- Interactive TCP client/server communication on `127.0.0.1:1234`
- IPv4 hostname resolution using `getaddrinfo()`
- Student ID → email lookup from a local, **synthetic** sample dataset
- Length-prefixed messages (4-byte network-order length + payload)
- Partial-send/receive handling and a 4096-byte message limit
- RAII-based socket lifetime management
- Automated integration tests through CTest and Python `unittest`

## Architecture

```text
client.cpp ── TCP length-prefixed protocol ── server.cpp
                 protocol.hpp                 │
                                         ┌───┴────────────────┐
                                         │                    │
                                 StudentDirectory       DNS lookup
                            student_directory.*       dns.*
                                         │
                                students_sample.txt

socket_raii.hpp: owns POSIX socket descriptors used by the client and server.
```

| File | Responsibility |
| --- | --- |
| `client.cpp` | Interactive TCP client |
| `server.cpp` | Listener, client sessions, request dispatch |
| `protocol.hpp` | Frame encoding/decoding, complete send/receive loops |
| `socket_raii.hpp` | Move-only RAII wrapper for POSIX socket descriptors |
| `student_directory.hpp/.cpp` | Load and look up records from a sample file |
| `dns.hpp/.cpp` | IPv4 hostname resolution |
| `tests/test_integration.py` | End-to-end network tests |
| `docs/` | Project technical report |

## Requirements

- Linux (tested in Ubuntu under WSL 2)
- C++17 compiler (such as GCC)
- CMake 3.16 or newer
- Python 3 for running the integration tests (standard library only)

This implementation uses POSIX sockets and does not build natively with Windows Winsock without changes.

## Build

Run from the repository root:

```bash
cmake -S . -B build
cmake --build build
```

This produces `build/server` and `build/client`.

## Run

The server reads `students_sample.txt` from its **working directory** by default. Start both programs from the repository root in separate terminals:

**Terminal 1**

```bash
./build/server
```

**Terminal 2**

```bash
./build/client
```

Menu options:

1. **DNS** — enter `localhost` to obtain `127.0.0.1`.
2. **QUERY** — enter `DEMO001` to obtain `alex@example.com` from synthetic sample data.
3. **QUIT** — end the client session.

The server can also receive an explicit dataset path as its first command-line argument.

## Automated tests

Stop any manually launched server first, because integration tests start their own instance on port `1234`.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -V
```

CTest launches one Python test suite with **nine integration test cases** covering:

- DNS resolution and blank hostname rejection
- Student lookup success and missing ID
- Invalid menu option
- TCP message fragmentation
- Reconnection after a normal disconnect
- Rejection of oversized frames followed by recovery
- Recovery after abrupt disconnect

These are functional integration tests, not performance or security certification.

## Engineering improvements

**Phase 1 — Reliability & Safety:** Added message framing, complete I/O loops, bounded message size, safer interactive input, improved disconnect handling, and synthetic demonstration data.

**Phase 2 — Modern C++ & Testing:** Introduced RAII socket ownership, extracted DNS and student-directory modules, adopted CMake, and registered automated Python integration tests with CTest.

## Current limitations

- Handles clients sequentially; no concurrent request processing.
- IPv4 and localhost-only binding by design.
- Student records are read from a simple text file; no database, authentication, or persistence of changes.
- DNS lookup is synchronous and can block while resolving names.
- No latency benchmark, load testing, TLS, or production hardening.

## Project history

Originally a Computer Networks final project (2023). The 2026 revisions focus on code quality, safety, repeatable builds, and testing. The historical commits remain in Git for context; the current source and synthetic dataset are the recommended starting point.

## License

No open-source license is currently provided. Please do not assume permission to reuse or redistribute this code unless a license is added.
