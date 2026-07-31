import socket
import threading
import time
import random
import string

HOST, PORT = "127.0.0.1", 6380
NUM_CLIENTS = 20
OPS_PER_CLIENT = 500

results_lock = threading.Lock()
total_ops = 0
errors = 0


def random_string(n=6):
    return "".join(random.choices(string.ascii_lowercase, k=n))


def client_worker(client_id):
    global total_ops, errors
    try:
        sock = socket.create_connection((HOST, PORT), timeout=5)
        local_ops = 0
        for i in range(OPS_PER_CLIENT):
            key = f"c{client_id}_k{i % 50}"  # reuse keys to trigger real cache hits/evictions
            if i % 3 == 0:
                cmd = f"SET {key} {random_string()}\n"
            else:
                cmd = f"GET {key}\n"
            sock.sendall(cmd.encode())
            resp = sock.recv(4096)
            if not resp:
                raise ConnectionError("empty response")
            local_ops += 1
        sock.close()
        with results_lock:
            total_ops += local_ops
    except Exception as e:
        with results_lock:
            errors += 1
        print(f"Client {client_id} error: {e}")


def main():
    threads = []
    start = time.time()
    for cid in range(NUM_CLIENTS):
        t = threading.Thread(target=client_worker, args=(cid,))
        threads.append(t)
        t.start()
    for t in threads:
        t.join()
    elapsed = time.time() - start

    print(f"\n--- Benchmark Results ---")
    print(f"Concurrent clients : {NUM_CLIENTS}")
    print(f"Total operations   : {total_ops}")
    print(f"Errors             : {errors}")
    print(f"Elapsed time       : {elapsed:.3f}s")
    if elapsed > 0:
        print(f"Throughput         : {total_ops / elapsed:,.0f} ops/sec")


if __name__ == "__main__":
    main()
