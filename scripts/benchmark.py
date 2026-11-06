#!/usr/bin/env python3
"""Latency benchmark for the IUT-Food TCP server.

Starts N concurrent worker connections, has each one log in and then
issue a realistic read-mostly workload (browse restaurants, open a
menu, check the wallet), and records the round-trip time of every
request.  Login times are reported separately because PBKDF2 hashing
intentionally costs CPU on the server.

Usage (against a scratch database so the numbers stay clean):
    ./build/iut-food-server --port 1234 --db /tmp/bench.db &
    python3 scripts/benchmark.py --clients 50 --per-client 20

Writes docs/benchmark-results.json so the README table stays honest.
"""
import argparse
import json
import socket
import statistics
import sys
import threading
import time


def recv_line(sock):
    buf = bytearray()
    while not buf.endswith(b"\n"):
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("server closed the connection")
        buf += chunk
    return json.loads(bytes(buf))


def send(sock, payload):
    sock.sendall((json.dumps(payload) + "\n").encode())


class Worker(threading.Thread):
    def __init__(self, host, port, requests, latencies, login_times):
        super().__init__(daemon=True)
        self.host, self.port = host, port
        self.requests = requests
        self.latencies = latencies  # shared lists, appended under the GIL
        self.login_times = login_times
        self.error = None

    def run(self):
        try:
            sock = socket.create_connection((self.host, self.port), timeout=30)
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

            t0 = time.perf_counter()
            send(sock, {"command": "login", "username": "demo_customer",
                        "password": "123456"})
            reply = recv_line(sock)
            self.login_times.append(time.perf_counter() - t0)
            if not reply.get("success"):
                raise RuntimeError("login failed in benchmark worker")

            send(sock, {"command": "get_restaurants"})
            restaurants = recv_line(sock)["data"]
            if not restaurants:
                raise RuntimeError("no seeded restaurants for the benchmark")
            restaurant_id = restaurants[0]["id"]

            workload = [
                {"command": "get_restaurants"},
                {"command": "get_menu_items_for_restaurant",
                 "restaurant_id": restaurant_id},
                {"command": "get_wallet_balance", "user_id": reply["user_id"]},
            ]
            for i in range(self.requests):
                payload = dict(workload[i % len(workload)])
                t = time.perf_counter()
                send(sock, payload)
                recv_line(sock)
                self.latencies.append((time.perf_counter() - t) * 1000.0)
            sock.close()
        except Exception as exc:  # noqa: BLE001 - report and mark the run dirty
            self.error = str(exc)


def percentile(sorted_values, p):
    if not sorted_values:
        return float("nan")
    k = (len(sorted_values) - 1) * p
    f, c = int(k), min(int(k) + 1, len(sorted_values) - 1)
    return sorted_values[f] + (sorted_values[c] - sorted_values[f]) * (k - f)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=1234)
    ap.add_argument("--clients", type=int, default=50)
    ap.add_argument("--per-client", type=int, default=20,
                    help="requests per client connection")
    ap.add_argument("--out", default="docs/benchmark-results.json")
    args = ap.parse_args()

    latencies, login_times = [], []
    workers = [Worker(args.host, args.port, args.per_client, latencies, login_times)
               for _ in range(args.clients)]

    print(f"benchmarking {args.host}:{args.port} with {args.clients} clients "
          f"x {args.per_client} requests ...")
    started = time.perf_counter()
    for w in workers:
        w.start()
    for w in workers:
        w.join()
    wall = time.perf_counter() - started

    failures = [w.error for w in workers if w.error]
    if failures:
        print(f"ERROR: {len(failures)} worker(s) failed: {failures[0]}",
              file=sys.stderr)
        return 1

    ordered = sorted(latencies)
    result = {
        "clients": args.clients,
        "requests_per_client": args.per_client,
        "total_requests": len(latencies),
        "wall_seconds": round(wall, 2),
        "requests_per_second": round(len(latencies) / wall, 1),
        "login_seconds_mean": round(statistics.mean(login_times), 3),
        "latency_ms": {
            "mean": round(statistics.mean(ordered), 2),
            "p50": round(percentile(ordered, 0.50), 2),
            "p90": round(percentile(ordered, 0.90), 2),
            "p95": round(percentile(ordered, 0.95), 2),
            "p99": round(percentile(ordered, 0.99), 2),
            "max": round(ordered[-1], 2),
        },
    }

    print(f"\n  requests      : {result['total_requests']}")
    print(f"  wall time     : {result['wall_seconds']} s")
    print(f"  throughput    : {result['requests_per_second']} req/s")
    print(f"  login (PBKDF2): {result['login_seconds_mean']} s mean")
    print("\n  latency (ms)   p50     p90     p95     p99     max")
    lat = result["latency_ms"]
    print(f"                 {lat['p50']:>6}  {lat['p90']:>6}  {lat['p95']:>6}  "
          f"{lat['p99']:>6}  {lat['max']:>6}")

    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2)
    print(f"\nsaved {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
