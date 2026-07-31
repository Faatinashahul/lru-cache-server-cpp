# LRU Cache Server (C++)

A multithreaded, in-memory LRU cache server, built from scratch in C++ —
conceptually similar to a minimal Redis. Built to demonstrate practical
command of C++, core data structures & algorithms, and systems design
(concurrency, networking, thread pooling).

## Why this project

Most "I know DSA" claims are backed by LeetCode problems, which are real
but isolated. This project instead builds a **working system** end-to-end:
a cache eviction policy implemented from first principles, wrapped in a
real TCP server that handles many simultaneous clients safely.

## Architecture

```
Client 1 ─┐
Client 2 ─┼─> TCP Server (accept loop) ─> ThreadPool (8 workers)
Client N ─┘                                     │
                                                 v
                                  Protocol (parses SET/GET/DELETE)
                                                 │
                                                 v
                                  ThreadSafeLRUCache (mutex-guarded)
                                                 │
                                                 v
                        LRUCache: unordered_map + doubly linked list
```

### Core data structure — `lru_cache.h`
- `unordered_map<key, Node*>` for O(1) lookup by key.
- A hand-rolled doubly linked list ordered by recency (head = most
  recently used, tail = least recently used).
- `get()` and `put()` are both O(1) — the map finds the node, and the
  list lets us reorder or evict without scanning anything.
- Each node stores its own key, so evicting the tail can also erase the
  correct entry from the map in O(1) (otherwise eviction would require
  an O(n) reverse lookup).

### Thread safety — `thread_safe_lru_cache.h`
- A single mutex guards the whole cache. This is a deliberate design
  choice: an LRU cache mutates shared list pointers on *every* access
  (even reads, because of `moveToFront`), so fine-grained per-node
  locking would need to lock multiple adjacent nodes just to unlink one
  — added complexity without a clear win at this scale. Coarse-grained
  locking is what most real-world LRU implementations do too.

### Concurrency — `thread_pool.h`
- A fixed pool of worker threads (default: 8) pulls jobs from a shared
  queue. Avoids spawning an unbounded number of OS threads under high
  connection load.

### Protocol — `protocol.h`
Plain-text, line-based protocol (testable with `nc`/`telnet`):
```
SET <key> <value>   -> OK
GET <key>           -> <value> | (nil)
DELETE <key>        -> OK | (nil)
PING                -> PONG
```

### Server — `server.cpp`
- Raw POSIX sockets (`socket`/`bind`/`listen`/`accept`).
- `TCP_NODELAY` set on each client socket to disable Nagle's algorithm,
  reducing latency for the small, frequent packets typical of a cache
  protocol.
- Each accepted connection is handed to the thread pool rather than
  blocking the accept loop.

## Build & run

```bash
g++ -std=c++17 -Wall -O2 -pthread -o server server.cpp
./server 6380
```

Test manually:
```bash
nc 127.0.0.1 6380
SET foo bar
GET foo
DELETE foo
GET foo
```

## Correctness tests

```bash
g++ -std=c++17 -Wall -o test_lru test_lru.cpp
./test_lru
```
Covers: basic get/put, LRU eviction order, updating an existing key,
and size tracking.

## Benchmark

```bash
python3 benchmark.py
```
Spawns 20 concurrent client threads issuing 500 mixed GET/SET operations
each (10,000 total ops) against the live server.

**Measured result (this environment):** 10,000 operations across 20
concurrent clients, 0 errors, ~60,000 ops/sec.
