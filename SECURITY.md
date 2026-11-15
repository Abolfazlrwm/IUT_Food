# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| 2.6.x   | ✅ |
| 2.5.x   | ✅ |
| 2.4.x   | ⚠️ security fixes only |
| ≤ 2.3.x | ❌ upgrade — the pre-2.4 wire protocol is fine, but releases before 2.4 lack the tuned/audited data layer and releases before 2.2 have **no session authentication at all** |

## Security posture (what actually ships)

These are real, tested properties — see `tests/` and `scripts/smoke_test.py`
for the verification:

- **Session-token authentication** (v2.2): every protocol command except
  `login` requires a server-minted 256-bit token (OS CSPRNG) with a
  30-minute sliding idle lifetime; `logout` revokes it. Roles and
  ownership are enforced per command — a client-sent `user_id` is never
  trusted.
- **Hardened password storage** (v2.1): PBKDF2-HMAC-SHA256 with a 16-byte
  random salt per user and 30,000 iterations, constant-time comparison,
  and a transparent in-place upgrade of the pre-2.1 unsalted hashes.
- **Transport encryption, opt-in** (v2.3): start the server with
  `--tls-cert/--tls-key` and the whole protocol travels inside
  TLS 1.2/1.3; plain-text clients are rejected during the handshake, and
  CI runs the full smoke suite a second time over TLS.
- **Protocol guards**: per-connection token-bucket rate limiting, a
  64 KiB frame cap, prepared statements for every query.
- **Audit trail** (v2.5): `--audit-file` appends one JSON object per
  security-relevant event (logins and failures, moderation, wallet
  movements, placed orders) — `jq` is already a working report.
- **Backups** (v2.5): scheduled `VACUUM INTO` snapshots with retention,
  plus a final snapshot on graceful shutdown.

## Known limitations

- **Without `--tls-cert/--tls-key`, credentials travel in the clear** —
  the wire protocol is newline-delimited JSON over plain TCP, so run TLS
  for anything beyond a localhost demo.
- The TLS story assumes a **self-signed or private PKI demo**; there is
  no certificate pinning on the client beyond the explicit "I trust this
  self-signed certificate" consent switch.
- The desktop client is a trusted demo client: the customer checkout
  path can also run **offline** against its local SQLite cache, in which
  case those orders never reach the server's audit trail until the
  broadcast round-trip (`notify_order_placed`).
- Admin endpoints return unbounded lists (no pagination) — fine for a
  campus, not for ten-thousand-row tables.

## Reporting a vulnerability

This is a student project, but real bugs deserve real reports. If you find a
security-relevant issue (SQL injection, authentication bypass, wallet
overdraw, protocol abuse…), please open a
[private security advisory](https://github.com/Abolfazlrwm/IUT_Food/security/advisories/new)
or email the address on the [GitHub profile](https://github.com/Abolfazlrwm) —
please **do not** open a public issue for exploitable problems.

If you want a fix acknowledged, include the protocol exchange that
triggers the issue (`scripts/smoke_test.py` is a good template for
crafting one).
