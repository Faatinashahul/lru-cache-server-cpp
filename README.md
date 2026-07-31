# LRU Cache Server (C++)

A multithreaded, in-memory LRU cache server, built from scratch in C++ —
conceptually similar to a minimal Redis. Built to demonstrate practical
command of C++, core data structures & algorithms, and systems design
(concurrency, networking, thread pooling).

## Problem Statement

Modern applications and websites handle far more read requests than their databases can efficiently serve on their own. Every time a database is queried for the same frequently-accessed data — a user profile, a product listing, a session token — it adds latency and load, even if that same data was just fetched moments ago.

The standard solution is an in-memory cache: a layer that sits between the application and the database, holding frequently-accessed data in RAM so it can be served almost instantly instead of hitting the database every time. But an in-memory cache has a hard constraint — memory is limited. It can't store everything forever, so it needs:

A fixed capacity with an intelligent policy for deciding what to discard when full (this project uses LRU — Least Recently Used — evicting whatever hasn't been accessed in the longest time).
Fast operations — lookups, insertions, and evictions all need to happen in constant time (O(1)), or the cache itself becomes a bottleneck instead of a solution.
Safe concurrent access — in real systems, many clients/requests hit the cache at the same time. Without careful design, simultaneous reads and writes can corrupt the underlying data structure or return incorrect results.
Network accessibility — a cache is only useful as shared infrastructure if multiple separate services or clients can connect to it remotely, not just use it as a local in-process object.

This project builds exactly that from scratch in C++, addressing all four requirements above.
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
