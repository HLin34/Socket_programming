# TCP Client–Server Application — Technical Report

**Project history:** Original Computer Networks coursework (2023); reliability, architecture and performance-oriented revisions (2026).
**Platform:** Linux / WSL Ubuntu, POSIX TCP sockets, C++17, CMake, Python standard-library tests.
**Document scope:** Describes the Phase 3 branch prior to final merge and release.

## Executive summary

This project is an educational localhost TCP client/server offering hostname resolution and a synthetic student-ID lookup. Work progressed through three distinct phases: (1) safe framed communication, (2) resource ownership and modular testability, and (3) bounded multi-client handling, graceful shutdown, structured logging, and repeatable application-level benchmark measurements. The server's maximum active-client capacity is four. This is not a production-grade trading networking stack.

## 1. System design

### 1.1 Request flow

1. Client connects to loopback `127.0.0.1:1234`.
2. Server accepts the socket; a bounded worker handles each active connection.
3. Application messages use a 4-byte unsigned network-order length followed by payload, with a 4,096-byte maximum payload.
4. The interactive menu offers DNS lookup, synthetic student lookup, or quit.
5. A worker releases socket ownership on completion. Slots are reclaimed to admit later connections.

```
Client(s) —TCP framing→ listener → bounded worker handler
                                     ├── dns.* / getaddrinfo()
                                     └── StudentDirectory / synthetic text file
                         RAII sockets · Logger · shutdown coordination
```

The server is sequential **within** each connection and concurrent **across** client connections. Capacity is deliberately limited. A fifth connection while four are occupied receives a busy response instead of spawning an unlimited worker.

### 1.2 Module responsibilities

| Module | Role |
| --- | --- |
| `protocol.hpp` | `send_all` and `receive_all` deal with partial I/O and interrupted syscalls; length-prefixed messages |
| `socket_raii.hpp` | Move-only POSIX file-descriptor wrapper; destructor closes the owned socket |
| `student_directory.*` | Loads synthetic records and looks up an ID via an encapsulated container |
| `dns.*` | Hostname validation and IPv4 `getaddrinfo()` resolution |
| `server.cpp` | Listener, bounded worker coordination, request dispatch, termination |
| `client.cpp` | Interactive request/response client |
| `logger.hpp` | Mutex-protected, single-line JSON event output to stderr |

## 2. Evolution and rationale

### Phase 1 — reliable message handling

The original implementation was updated to use explicit 32-bit length framing and complete send/receive loops. This matters because TCP transports a byte stream rather than preserving application message boundaries. The design also bounds payload size, handles errors/disconnection and replaces historical data with synthetic examples in the current tree.

### Phase 2 — modern C++ and testability

RAII ties socket file descriptors to object lifetime, reducing the risk of early-return leaks and double closes from accidental copying. DNS and student-data functions were extracted into distinct compilation units. CMake captures build dependencies; Python `unittest` scripts under CTest exercise actual socket interactions.

### Phase 3 — bounded concurrency and observability

The listener permits up to four active worker handlers using `std::thread`. The concurrent handler path moves socket ownership into its worker. This is a bounded thread-per-connection design, not a pool or async event loop.

Graceful termination responds to SIGINT/SIGTERM through a minimal stop notification, then coordinates cleanup outside the signal handler: stop admitting work, call `shutdown()` on active sockets and join workers. A tested SIGINT case demonstrates clean exit with four idle clients; it does not establish behavior under every blocking DNS condition.

Structured logging emits UTC timestamped JSON Lines with event type, severity and client ID. Mutex serialization prevents multiple workers from interleaving partial log records. The events include connection, lookup type, invalid option, busy rejection, disconnect and stopping/stopped notifications. Request arguments, IDs and email values are intentionally not logged.

## 3. Validation

### Build and CI

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The recorded WSL checks passed all three configured CTest suites. The test inventory comprises nine application/protocol cases, five concurrency cases and one standalone SIGINT shutdown check. The Phase 3 branch also showed a successful GitHub Actions `C++ Build and Test` workflow on an Ubuntu runner (commit `cec38b3`).

| Test area | Examples | Known boundary |
| --- | --- | --- |
| Protocol / behavior | localhost DNS, missing ID, invalid option, fragmented frame, oversized frame, reconnect | Does not prove all malformed inputs safe |
| Concurrency | Four simultaneous clients, fifth rejected, capacity reuse, abrupt disconnect | Does not constitute formal race detection |
| Shutdown | SIGINT with four idle clients, orderly process exit | DNS-in-progress shutdown not separately validated |

### Observability checks

Local manual checks confirmed valid JSON event lines for startup, connection, DNS/student requests, quit/disconnect and shutdown. Logger serialization is implemented with `std::mutex`; no dedicated stress test of log ordering or completeness under high contention is claimed.

## 4. Performance benchmark

### Method

The Python benchmark launches the C++ server on loopback and issues closed-loop student-lookup requests over established sockets. Each client sends one complete interactive request before issuing the next. The study uses 1, 2 and 4 active clients, 30 warm-ups and 200 measured requests **per client per run**, with three runs at each load. The reported row entries are **medians of the per-run statistics**, not percentiles computed from concatenated samples.

The timed application exchange covers menu/prompt frames and request/response; initial connection establishment is excluded. Throughput counts measured successful completions divided by elapsed time, with a small scheduling/start offset noted in the benchmark report.

### Recorded results — October 30, 2026

| Clients | RPS (median) | P50 ms (median) | P95 ms (median) | P99 ms (median) |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 11.31 | 88.0039 | 91.9753 | 92.1352 |
| 2 | 22.64 | 88.0234 | 91.9142 | 92.1839 |
| 4 | 45.27 | 88.0082 | 91.8354 | 92.2020 |

At four concurrent clients, measured throughput was approximately 4× the single-client case, while recorded P50/P99 values stayed near 88/92 ms. These observations support increased overlap/concurrency for **this** fixed, closed-loop workload, not a general scalability claim.

### Interpretation and limitations

- The benchmark reflects **application-level request response time**, not physical network RTT or server-only processing time.
- Python client overhead, local WSL scheduling, server JSON logging and multiple small message exchanges all contribute to measured values.
- A stable ~88 ms median motivates protocol-level investigation. Delayed ACK and Nagle behavior are plausible explanations to test, **not established findings**.
- This experiment does not test external network conditions, live DNS resolution latency, exchange traffic, TLS, burst arrival patterns or unbounded concurrent load.
- The reported values should **not** be labeled ultra-low latency or HFT-grade.

## 5. How to reproduce

From the repository root in WSL Ubuntu:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
python3 benchmarks/benchmark.py --server ./build/server --iterations 200 --warmup 30 --runs 3
```

Stop other servers bound to port `1234` beforehand. The benchmark produces files under `benchmark_results/`, which should be excluded from Git. The synthetic sample input must be present relative to the server's working directory.

## 6. Known limitations and possible extensions

The service deliberately uses a four-client cap, blocking per-connection threads, IPv4 loopback only and synchronous DNS. It does not include authentication, TLS, persistent storage, fault-injection coverage, sanitizer/race-detector evidence, or a production deployment configuration. Future work, if desired, could include a measured `TCP_NODELAY` comparison, ThreadSanitizer experiments and extended shutdown tests for DNS-in-progress cases. None of these is presented as already implemented.

## 7. Portfolio takeaway

This project demonstrates a progression from a working student networking assignment to a more maintainable and measurable C++ application: reliable protocol boundaries, explicit resource ownership, modular builds, practical concurrency controls, regression testing, observable behavior and cautious interpretation of measured performance.
