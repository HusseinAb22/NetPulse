#!/usr/bin/env python3
"""NetPulse interactive chat client.

Usage:
    python3 chat_client.py [host] [port]

Defaults: host=127.0.0.1, port=9000.

Reads protocol lines from the server in a background thread and pretty-prints
them; reads stdin in the main thread and forwards lines to the server.
Type `/quit` (or Ctrl-D) to exit.
"""

import socket
import sys
import threading
from datetime import datetime

HOST_DEFAULT = "127.0.0.1"
PORT_DEFAULT = 9000


# ── ANSI colors (degrade gracefully when stdout isn't a TTY) ───────────────
class Color:
    RESET = "\033[0m"
    DIM = "\033[2m"
    BOLD = "\033[1m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"

    USE = sys.stdout.isatty()


def c(color: str, text: str) -> str:
    return f"{color}{text}{Color.RESET}" if Color.USE else text


def ts() -> str:
    return c(Color.DIM, f"[{datetime.now().strftime('%H:%M:%S')}]")


# ── Server → client line formatting ────────────────────────────────────────
def render_server_line(line: str) -> str:
    """Pretty-print one protocol line from the server."""
    if not line:
        return ""
    verb, _, rest = line.partition(" ")

    if verb == "OK":
        return f"{ts()} {c(Color.GREEN, 'ok')}    {rest}"
    if verb == "ERR":
        return f"{ts()} {c(Color.RED, 'err')}   {rest}"
    if verb == "BROADCAST":
        room, _, tail = rest.partition(" ")
        sender, _, body = tail.partition(" ")
        return f"{ts()} {c(Color.CYAN, room)}  {c(Color.BOLD, sender)}: {body}"
    if verb == "PRIVMSG":
        recipient, _, tail = rest.partition(" ")
        sender, _, body = tail.partition(" ")
        return f"{ts()} {c(Color.MAGENTA, '(DM)')}  {c(Color.BOLD, sender)}: {body}"
    if verb == "ROOMLIST":
        return f"{ts()} {c(Color.YELLOW, 'rooms')} {rest}"
    return f"{ts()} {line}"


# ── Local echo of what the user sent (server doesn't echo MSG/DM back) ─────
def render_local_echo(line: str) -> str | None:
    parts = line.split(" ", 2)
    verb = parts[0].upper() if parts else ""

    if verb == "MSG" and len(parts) == 3:
        room, body = parts[1], parts[2]
        return (f"{ts()} {c(Color.CYAN, room)}  "
                f"{c(Color.BOLD + Color.GREEN, 'me')}: {body}")
    if verb == "DM" and len(parts) == 3:
        target, body = parts[1], parts[2]
        return (f"{ts()} {c(Color.MAGENTA, f'(→{target})')}  "
                f"{c(Color.BOLD + Color.GREEN, 'me')}: {body}")
    return None


# ── Background reader: drain socket, print whole lines ─────────────────────
def reader_loop(sock: socket.socket, stop: threading.Event) -> None:
    buf = b""
    while not stop.is_set():
        try:
            chunk = sock.recv(4096)
        except (ConnectionResetError, OSError):
            break
        if not chunk:
            break
        buf += chunk
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            line = raw.rstrip(b"\r").decode("utf-8", errors="replace")
            out = render_server_line(line)
            if out:
                # Erase the half-typed prompt, print the incoming line, redraw.
                sys.stdout.write("\r\x1b[2K" + out + "\n> ")
                sys.stdout.flush()
    sys.stdout.write("\r\x1b[2K" + c(Color.RED, "* server disconnected") + "\n")
    sys.stdout.flush()
    stop.set()


# ── Main: connect, spawn reader, drive stdin → socket ──────────────────────
def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else HOST_DEFAULT
    port = int(sys.argv[2]) if len(sys.argv) > 2 else PORT_DEFAULT

    print(f"connecting to {host}:{port}...")
    try:
        sock = socket.create_connection((host, port))
    except OSError as e:
        print(c(Color.RED, f"connection failed: {e}"))
        return 1
    print(c(Color.DIM,
            "commands: NICK / JOIN / MSG / DM / LIST / QUIT.  "
            "/quit or Ctrl-D to exit."))

    stop = threading.Event()
    threading.Thread(target=reader_loop, args=(sock, stop), daemon=True).start()

    try:
        while not stop.is_set():
            try:
                line = input("> ")
            except EOFError:
                break
            line = line.strip()
            if not line:
                continue
            if line in ("/quit", "/q"):
                break

            echo = render_local_echo(line)
            if echo:
                sys.stdout.write("\r\x1b[2K" + echo + "\n")
                sys.stdout.flush()

            try:
                sock.sendall((line + "\n").encode("utf-8"))
            except OSError:
                print(c(Color.RED, "send failed; disconnecting"))
                break
    except KeyboardInterrupt:
        pass

    try:
        sock.sendall(b"QUIT\n")
    except OSError:
        pass
    sock.close()
    stop.set()
    print("bye.")
    return 0


if __name__ == "__main__":
    sys.exit(main())