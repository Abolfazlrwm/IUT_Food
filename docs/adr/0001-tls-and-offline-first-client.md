# ADR-0001: Opt-in TLS transport and offline-first client design

- **Status:** Accepted
- **Date:** 2026-11-20
- **Scope:** network transport and client state design for the IUT_Food client/server pair
- **Related:** `docs/PROTOCOL.md` (wire protocol and session tokens), `docs/ARCHITECTURE.md`

## Context

IUT_Food is a campus food-ordering system: a Qt 6 / C++17 desktop client, a TCP
server speaking newline-delimited JSON on port 1234, and SQLite storage on both
sides. Two constraints shaped the transport and state design:

1. **The campus network is not trusted.** Credentials, wallet balances and order
   data cross the wire; a passive observer on the same LAN must not be able to
   read or forge them.
2. **The server will not always be reachable.** The client must stay useful
   (browse cached menus, review local state) when the server is down, and
   reconcile with it whenever it comes back.

Authentication was already solved at the protocol layer before this ADR: since
v2.2 every command except `login` carries a 256-bit session token minted from
the OS CSPRNG (32 random bytes, hex-encoded), with a 30-minute sliding idle
lifetime, per-session role, and ownership checks on wallets, orders, profiles
and menus.

## Decision

**D1 — TLS is the transport security, but opt-in.** The server accepts
`--tls-cert/--tls-key` and then serves the same newline-delimited JSON inside a
standard TLS 1.2/1.3 channel on the same port. `scripts/generate_certs.sh`
creates a self-signed localhost certificate for demos. The client login dialog
exposes a TLS switch, with an explicit consent step for self-signed demo
certificates and a TLS badge on encrypted sessions in the status bar.

Rationale: as a university project, TLS-by-default would have forced every demo
and grading setup to depend on a CA or to click through certificate warnings.
Making TLS opt-in keeps the plain-JSON path inspectable for networking coursework,
while the encrypted path is exercised by a dedicated unit suite plus a CI smoke run.

**D2 — the client is offline-first.** The client keeps a local SQLite cache
(`iut_food.db`) with tables mirroring the server domain, degrades gracefully when
the server is unreachable, and syncs through the server whenever it is available.
The local data layer is tuned for concurrent access (WAL journaling,
`synchronous=NORMAL`, busy timeout) so the UI stays responsive while writes land.

## Alternatives considered

- **Always-on TLS with pinned self-signed certificates** — a stronger default
  posture, but it complicates every demo/lab setup and was judged out of scope
  for the course timeline; a deployment that wants it is one flag away.
- **A custom Noise-style handshake instead of TLS** — educational, but a much
  larger surface to get right in Qt/C++ when OpenSSL bindings are not already in
  the stack; standard TLS gives audited cryptography for free.
- **Always-online client with no local cache** — simpler consistency model, but
  the app becomes useless exactly when the campus server restarts; offline-first
  was a product requirement, not an optimization.

## Consequences

**Positive**

- The encrypted path is standard TLS — no custom cryptography on the wire.
- The client never hard-fails because the server is down; the cache absorbs it.
- Session tokens and ownership checks give server-side authorization independent
  of transport security — defense in depth when TLS is off.

**Negative / accepted trade-offs**

- With TLS off (plain mode), traffic is readable on the LAN — acceptable only for
  local development and coursework demos, never for a real deployment.
- Self-signed demo certificates mean the demo TLS path authenticates by explicit
  user consent, not by a CA chain.
- Two sources of truth (local cache vs server) must be reconciled on reconnect;
  the sync path is a simple last-writer flow, not full CRDT merging.

## Verification

- Session-token authentication and sliding expiry: `docs/PROTOCOL.md`, section 1.
- TLS path: dedicated unit suite plus a CI smoke run over the encrypted channel.
- SQLite tuning flags (v2.4): documented in the README; the measurement harness
  and results live in `docs/METRICS.md` and `docs/benchmark-results.json`.
