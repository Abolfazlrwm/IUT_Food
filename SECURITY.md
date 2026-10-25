# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| 2.0.x   | ✅ |
| 1.0.x   | ⚠️ best effort |
| < 1.0   | ❌ |

## Reporting a vulnerability

This is a student project, but real bugs deserve real reports. If you find a
security-relevant issue (SQL injection, authentication bypass, wallet
overdraw, protocol abuse…), please open a
[private security advisory](https://github.com/Abolfazlrwm/IUT_Food/security/advisories/new)
or email the address on the [GitHub profile](https://github.com/Abolfazlrwm) —
please **do not** open a public issue for exploitable problems.

## Known limitations (by design, documented in the README roadmap)

- The wire protocol is plain TCP — no TLS yet; do not expose the server to
  the public internet.
- Passwords are hashed with unsalted SHA-256 (bcrypt/Argon2 is on the
  roadmap).
- The server trusts the commands it receives — there is no session-token
  authentication yet, so any connected client can send admin commands.
- Wallet deduction is transactional, but the demo protocol does not
  authenticate *who* is charging a wallet.
