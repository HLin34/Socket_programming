# Socket Programming — Phase 1 (Safety and Reliability)

This is a revised **local-only** C++ TCP client/server demonstration based on the original 2023 course project. The feature set is unchanged: DNS lookup, student ID lookup, and quit. This version uses **length-prefixed framing** and is **not wire-compatible** with the original binaries; rebuild both sides together.

## Prerequisites
Linux or Windows Subsystem for Linux (WSL), g++ with C++17 support.

## Build
```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic server.cpp -o server
g++ -std=c++17 -Wall -Wextra -Wpedantic client.cpp -o client
```

## Run
Open two terminals in this folder:

Terminal 1: `./server`

Terminal 2: `./client`

Use option `1` with `localhost` to test DNS; use option `2` with `DEMO001` to test the sample file; use `3` to quit. The server uses `students_sample.txt` by default. You may pass a different synthetic dataset file as `./server path/to/file`.

## Changes versus original
- Replaces raw 256-byte messages with a 4-byte big-endian length header and up to 4096 bytes of content.
- Handles partial I/O, EOF, interrupted system calls, and oversized payloads.
- Uses bounded `std::string` messages and file-backed `std::unordered_map` lookups.
- Closes each accepted connection before accepting the next client.
- Uses `getaddrinfo` and validates hostnames at a basic level.
- Binds only to loopback instead of all interfaces.
- Uses fictitious student IDs and email addresses for demonstration.

## Scope and limitations
- **One client at a time**, not concurrent.
- No authentication, TLS, remote deployment, or persistent database.
- TCP server and client are intended for local learning exercises, not production exposure.
- `getaddrinfo()` may use external DNS, depending on hostname.
- Original data file is intentionally excluded from this archive.

## Next steps
Phase 2: extract separate service modules, introduce RAII socket wrappers and CMake, and add automated tests.
