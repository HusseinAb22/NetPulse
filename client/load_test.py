#!/usr/bin/env python3
"""NetPulse load test.

Spawns N asyncio-driven clients. All clients NICK + JOIN one room and then,
once everyone is registered, each fires off M MSG lines back-to-back. Other
clients receive them as BROADCASTs and record end-to-end latency from a
high-resolution send timestamp embedded in the body.

Usage:
    python3 load_test.py [--host HOST] [--port PORT]
                         [--clients N] [--messages M] [--room ROOM]

Defaults: host=127.0.0.1 port=9000 clients=50 messages=100 room=#loadtest.
Requires Python 3.7+.
"""
import argparse
import asyncio
import statistics
import sys
import time


class State:
    def __init__(self, n: int):
        self.n = n
        self.ready = 0
        self.sent = 0
        self.errors = 0
        self.lock = asyncio.Lock()
        self.start_event = asyncio.Event()
        self.teardown_event = asyncio.Event()
        self.latencies_ms: list[float] = []


async def client(idx: int, args, state: State) -> None:
    name = f"u{idx}"
    try:
        reader, writer = await asyncio.open_connection(args.host, args.port)
    except OSError as e:
        print(f"  client {name} connect failed: {e}", file=sys.stderr)
        async with state.lock:
            state.errors += 1
            state.ready += 1
            state.sent += 1
        return

    join_ok = asyncio.Event()
    ok_count = 0  # expect 2: NICK ack + JOIN ack

    async def receiver():
        nonlocal ok_count
        while True:
            try:
                line = await reader.readline()
            except (OSError, asyncio.CancelledError):
                return
            if not line:
                return
            text = line.rstrip().decode("utf-8", errors="replace")
            if text.startswith("OK "):
                ok_count += 1
                if ok_count == 2:
                    join_ok.set()
                continue
            if not text.startswith("BROADCAST "):
                continue
            parts = text.split(" ", 3)
            if len(parts) < 4:
                continue
            body = parts[3]
            if not body.startswith("t="):
                continue
            try:
                token = body.split(" ", 1)[0]
                t_send_ns = int(token[2:])
                lat_ms = (time.perf_counter_ns() - t_send_ns) / 1e6
                state.latencies_ms.append(lat_ms)
                state.last_recv_ns = time.perf_counter_ns()
            except (ValueError, IndexError):
                pass

    recv_task = asyncio.create_task(receiver())

    # Setup: NICK + JOIN, wait for both OKs before reporting ready so we're
    # sure the dispatcher has actually registered us as a room member.
    writer.write(f"NICK {name}\nJOIN {args.room}\n".encode())
    await writer.drain()
    try:
        await asyncio.wait_for(join_ok.wait(), timeout=5.0)
    except asyncio.TimeoutError:
        async with state.lock:
            state.errors += 1
            state.ready += 1
            state.sent += 1
        recv_task.cancel()
        writer.close()
        return

    async with state.lock:
        state.ready += 1
    await state.start_event.wait()

    # Send burst — each MSG carries its send timestamp in nanoseconds.
    for i in range(args.messages):
        t = time.perf_counter_ns()
        writer.write(f"MSG {args.room} t={t} m{i}\n".encode())
    try:
        await writer.drain()
    except OSError:
        async with state.lock:
            state.errors += 1
    async with state.lock:
        state.sent += 1

    # Wait for the coordinator to give the all-clear after the settle window.
    await state.teardown_event.wait()
    try:
        writer.write(b"QUIT\n")
        await writer.drain()
    except OSError:
        pass
    recv_task.cancel()
    writer.close()
    try:
        await writer.wait_closed()
    except OSError:
        pass


async def run(args) -> None:
    state = State(args.clients)

    print("NetPulse load test")
    print(f"  target:              {args.host}:{args.port}")
    print(f"  clients:             {args.clients}")
    print(f"  messages per client: {args.messages}")
    print(f"  room:                {args.room}")
    print()

    tasks = [asyncio.create_task(client(i, args, state))
             for i in range(args.clients)]

    while state.ready < args.clients:
        await asyncio.sleep(0.02)
    print("  all clients ready, starting send phase...")
    t0 = time.perf_counter()
    state.start_event.set()

    while state.sent < args.clients:
        await asyncio.sleep(0.02)

    await asyncio.sleep(args.settle)
    t1 = time.perf_counter()

    state.teardown_event.set()
    await asyncio.gather(*tasks, return_exceptions=True)

    real_window = (state.last_recv_ns / 1e9) - t0

    elapsed = t1 - t0
    total_sent = args.clients * args.messages
    total_received = len(state.latencies_ms)
    #throughput = total_received / elapsed if elapsed > 0 else 0.0
    throughput = total_received / real_window

    print()
    print(f"  duration:                {elapsed:.2f}s  (incl. {args.settle:.1f}s settle)")
    print(f"  MSG lines sent:          {total_sent:,}")
    print(f"  BROADCASTs received:     {total_received:,}")
    print(f"  throughput:              {throughput:,.0f} broadcasts/sec")
    print(f"  errors:                  {state.errors}")

    if state.latencies_ms:
        lats = sorted(state.latencies_ms)

        def pct(p: float) -> float:
            i = min(int(len(lats) * p / 100), len(lats) - 1)
            return lats[i]

        print()
        print("  end-to-end broadcast latency (ms):")
        print(f"    p50   = {pct(50):.2f}")
        print(f"    p95   = {pct(95):.2f}")
        print(f"    p99   = {pct(99):.2f}")
        print(f"    max   = {max(lats):.2f}")
        print(f"    mean  = {statistics.fmean(lats):.2f}")


def main() -> int:
    parser = argparse.ArgumentParser(description="NetPulse load test")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9000)
    parser.add_argument("--clients", type=int, default=50)
    parser.add_argument("--messages", type=int, default=100)
    parser.add_argument("--room", default="#loadtest")
    parser.add_argument("--settle", type=float, default=1.5,
                        help="seconds to wait for in-flight broadcasts after the last send")
    args = parser.parse_args()
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())