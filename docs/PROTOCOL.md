# NetPulse Wire Protocol

A simple text-based, line-delimited protocol over TCP. All messages are ASCII, terminated by a single newline (`\n`); a trailing carriage return (`\r`) is tolerated and stripped. The server listens on TCP port 9000 by default.

## Framing

- One command per line.
- Terminator: `\n` (LF). The server also accepts `\r\n` (CRLF) — the `\r` is stripped by the line framer.
- Maximum buffered line length: **16 KiB**. A client that exceeds this without sending `\n` is disconnected.
- Fields within a line are separated by single ASCII spaces. The trailing free-text field (the message body) may itself contain spaces — everything after the verb's fixed-position arguments is treated as one body.

## Session lifecycle

A client **must** claim a nickname with `NICK` before any other command except `QUIT`. Without one, the server replies `ERR set a nickname first (NICK <name>)`.

```
connect ──▶ NICK <name>  ──▶ JOIN / MSG / DM / LIST  ──▶ QUIT  ──▶ disconnect
                                       │
                                       └──▶ NICK <newname>   (IRC-style nick change)
```

### Nickname rules

- Non-empty.
- No whitespace (the protocol is space-delimited).
- At most 32 characters.
- Claiming a nick currently held by **another** connection returns `ERR nickname taken`.
- Re-claiming your own nick is idempotent (returns `OK`).
- A successful `NICK` while you already hold a different nick frees the old one atomically.

## Commands (client → server)

| Verb   | Syntax                | On success                                                                                                | Notes |
|--------|-----------------------|-----------------------------------------------------------------------------------------------------------|-------|
| `NICK` | `NICK <name>`         | `OK nick <name>`                                                                                          | Required before any non-`QUIT` command. |
| `JOIN` | `JOIN <#room>`        | `OK joined <#room>`; the joiner is then replayed up to 50 recent room messages; existing members hear `BROADCAST <#room> server <nick> joined`. | Creates the room if it does not exist. |
| `MSG`  | `MSG <#room> <text>`  | Each other member of the room receives `BROADCAST <#room> <sender> <text>`. No `OK` to the sender.        | Sender must already be a member. The sender is *not* echoed their own message; clients should render sent messages locally. |
| `DM`   | `DM <nick> <text>`    | Recipient receives `PRIVMSG <recipient> <sender> <text>`. No `OK` to the sender.                          | Recipient must be online (registered nick). |
| `LIST` | `LIST`                | `ROOMLIST <#room1> <#room2> ...`                                                                          | Lists all currently known rooms. |
| `QUIT` | `QUIT`                | (no reply — server closes the connection)                                                                 | Server releases the nick, leaves all rooms, closes the socket. |

## Replies (server → client)

| Verb        | Syntax                                          | When sent                                            |
|-------------|-------------------------------------------------|------------------------------------------------------|
| `OK`        | `OK <context>`                                  | Acknowledgement of a client command (e.g. `OK joined #general`). |
| `ERR`       | `ERR <reason>`                                  | Any rejected command. The reason is human-readable text. |
| `BROADCAST` | `BROADCAST <#room> <sender> <text>`             | A `MSG` was delivered to a room you're in, **or** a server-generated room event (e.g. `BROADCAST #general server bob joined`). |
| `PRIVMSG`   | `PRIVMSG <recipient> <sender> <text>`           | A `DM` was directed to you. `<recipient>` is your own nick. |
| `ROOMLIST`  | `ROOMLIST <#room1> <#room2> ...`                | Reply to a `LIST` command.                            |

## Error reasons

The following `ERR <reason>` strings are emitted by the current server. Clients may parse the reason for logic or treat the whole line as opaque human-readable text.

| Reason                                            | Cause                                                  |
|---------------------------------------------------|--------------------------------------------------------|
| `invalid nickname`                                | Empty, longer than 32 chars, or contains whitespace.   |
| `nickname taken`                                  | Another connection holds this nickname.                |
| `set a nickname first (NICK <name>)`              | Sent a non-`QUIT` command before `NICK`.               |
| `no such room <#room>`                            | `MSG` to a room that doesn't exist.                    |
| `not in room <#room>`                             | `MSG` to a room you haven't `JOIN`ed.                  |
| `user not found <nick>`                           | `DM` to an unknown or offline nickname.                |
| `protocol violation`                              | Client sent a server-only verb (`BROADCAST`, `PRIVMSG`, `OK`, `ERR`, `ROOMLIST`). |

Malformed lines (unknown verbs, missing arguments) are silently dropped — the server logs them and reads on. This is so a desynced client can't lock the connection with a `recv` that never advances.

## Example session

```
C: NICK alice
S: OK nick alice
C: JOIN #general
S: OK joined #general
C: MSG #general hi everyone

(meanwhile, bob connects)
C': NICK bob
S: OK nick bob
C': JOIN #general
S: OK joined #general
S: BROADCAST #general alice hi everyone        ← replayed history (sent to bob)
   (alice receives) BROADCAST #general server bob joined

C: DM bob can you review my PR?
   (bob receives) PRIVMSG bob alice can you review my PR?

C: LIST
S: ROOMLIST #general

C: QUIT
   (server closes the connection)
```

## Out of scope / future work

- **Timestamps on the wire.** Each stored message carries a server-side `timestamp` already (used internally for history); the wire format does not yet transmit it. Once added, clients will be able to render time markers — e.g. one every 10–20 messages — without baking display policy into the server.
- **`USERLIST <#room> <user1> <user2> ...`** — listed in the original design but not yet implemented. Adding it requires a client-side trigger verb (e.g. `WHO #room`).
- **Typed server-initiated close** (`BYE <reason>`) — today the server simply closes the socket; a typed reason would let clients render a friendly message.

## Versioning

This protocol is currently unversioned. Future **incompatible** changes will introduce a `VERSION` handshake. Backwards-compatible additions (new verbs, new error reasons) will be added without a version bump.