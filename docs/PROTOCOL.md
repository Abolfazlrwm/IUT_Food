# IUT-Food Network Protocol

The client and the server speak **newline-delimited JSON over TCP**
(default port `1234`). Every request is one line ending with `\n`;
every response is one line ending with `\n`.

- Requests carry a `"command"` field.
- Responses echo `"command"` and set `"type": "response"` (or a
  dedicated `type` for pushed data).
- Push notifications (`*_notification`, `restaurant_list_response`) are
  sent **without** a matching request — the client dispatches them by
  their `type` field.
- Since **v2.2** every command except `login` must carry the session
  token issued by the server (see §1).

---

## 1. Authentication (v2.2)

The wire protocol is **session-token authenticated**. `login` is the
only unauthenticated command; a successful login mints a 256-bit
token (32 bytes from the OS CSPRNG, hex-encoded) that represents the
signed-in identity `(user_id, role, username)`.

### `login`
```json
→ {"command": "login", "username": "demo_customer", "password": "123456"}

← {"type": "response", "command": "login", "success": true,
   "role": "customer", "user_id": 2,
   "session_token": "9f86d081884c7d65…(64 hex chars)",
   "expires_in": 1800}
```

- `session_token` — present only on success. Attach it to every
  subsequent request as the `"token"` field.
- `expires_in` — idle lifetime in seconds (default 1800 = 30 min).
  The window is **sliding**: each accepted request refreshes it.
- Passwords are stored as salted PBKDF2-HMAC-SHA256 on the server
  (30 000 iterations); the raw password is only ever visible on the
  wire of this demo protocol (see Roadmap).

### `logout`
Revokes the presented token immediately. Any later use of it answers
`unauthorized`.
```json
→ {"command": "logout", "token": "…"}
← {"type": "response", "command": "logout", "success": true}
```

### Authorization model
The server **never trusts client-sent ids**. `user_id` /
`customer_id` / `restaurant_id` fields are cross-checked against the
session; ownership checks read the database.

| Scope | Commands | Rule |
|---|---|---|
| public | `login` | no token required |
| any session | `get_restaurants`, `validate_coupon`, `send_chat_message`, `notify_order_placed`, `logout` | valid token |
| customer (self) | `place_order`, `get_wallet_balance`, `charge_wallet`, `pay_from_wallet`, `submit_review`, `update_profile` | the ids in the request must match the session's `user_id` |
| restaurant (own) | `add_menu_item`, `update_menu_item`, `delete_menu_item`, `get_orders_for_restaurant` | `restaurant_id` must equal the session's `user_id`; menu edits verify the item belongs to the caller |
| restaurant + customer | `update_order_status` | only for orders they own (customers confirm/cancel their own order, restaurants their own queue; admins may steer any order) |
| admin only | `get_users`, `block_user`, `unblock_user`, `delete_user`, `approve_restaurant`, `disapprove_restaurant`, `get_orders`, `get_reviews`, `get_coupons`, `create_coupon`, `toggle_coupon`, `get_server_stats` | session role must be `admin` |

Failures answer `unauthorized` (no/invalid/expired token) or
`forbidden` (valid token, insufficient role or ownership):
```json
← {"type": "response", "command": "get_users", "success": false,
   "error": "unauthorized", "message": "Authentication required: …"}
← {"type": "response", "command": "block_user", "success": false,
   "error": "forbidden", "message": "Admin role required."}
```

Sessions live in server memory (never persisted); a server restart
simply means the clients log in again.

### `get_server_stats` (admin)
Operational counters, useful with the benchmark harness:
```json
→ {"command": "get_server_stats", "token": "…"}

← {"type": "response", "command": "get_server_stats", "success": true,
   "uptime_seconds": 372, "active_sessions": 3, "sessions_opened_total": 9,
   "connections_accepted": 11, "requests_served": 214,
   "requests_rejected": 0, "auth_failures": 2}
```

## 2. Restaurants & menu

### `get_restaurants`
Filters are optional; the literal values `"همه"` / `"All"` / `""` mean
*no filtering*.
```json
→ {"command": "get_restaurants", "name": "", "type": "فست فود", "location": ""}

← {"type": "restaurant_list_response", "command": "get_restaurants",
   "data": [
     {"id": 4, "name": "پیتزا پاردیس", "type": "فست فود",
      "location": "منطقه 2", "price_range": 1}
   ]}
```

### `get_menu_items_for_restaurant`
```json
→ {"command": "get_menu_items_for_restaurant", "restaurant_id": 4}

← {"type": "response", "command": "get_menu_items_for_restaurant",
   "success": true,
   "data": [{"id": 5, "name": "پیتزا پپرونی", "price": 320000.0,
             "description": "خمیر ایتالیایی…"}]}
```

### `add_menu_item` / `update_menu_item` / `delete_menu_item`
```json
→ {"command": "add_menu_item", "restaurant_id": 4,
   "name": "پیتزا چهارپنیر", "price": 350000, "description": "…"}

← {"type": "response", "command": "add_menu_item", "success": true}
```
`update_menu_item` and `delete_menu_item` address the dish by `"id"`.

## 3. Ordering

Order states move through the canonical machine
`pending → confirmed → shipped → delivered` (`cancelled` is reserved).
The UI translates them to Persian via `orderstatus.h`.

### `place_order`
The **server** assigns the order id and `created_at`; `status` is forced
to `pending`.
Optional v2 fields: `payment_method` (`"cash"` default / `"wallet"`) and
`coupon_code` — when present the coupon is validated and consumed
server-side and the discount is stored with the order.
```json
→ {"command": "place_order", "customer_id": 2, "restaurant_id": 4,
   "total_price": 410000, "payment_method": "wallet",
   "coupon_code": "WELCOME20",
   "items": [{"id": 5, "name": "پیتزا پپرونی", "price": 320000, "quantity": 1}]}

← {"type": "response", "command": "place_order", "success": true,
   "order_id": 3, "total_price": 328000}
```
With `payment_method: "wallet"` the deduction is atomic; an insufficient
balance answers `success: false` + `"error": "موجودی کیف پول کافی نیست."`
and the just-created order is marked `cancelled`.

### `notify_order_placed`
Clients that write orders to the shared database directly (offline-first)
use this **informational broadcast** so the other live sessions learn
about the order without re-inserting it:
```json
→ {"command": "notify_order_placed", "order_id": 12, "restaurant_id": 4,
   "total_price": 328000, "customer": "demo_customer"}

⟳ push to all other clients:
{"type": "new_order_notification",
 "data": {"order_id": 12, "restaurant_id": 4, "total_price": 328000,
          "customer": "demo_customer"}}
```

### `get_orders`
With `"restaurant_id"` → only that restaurant's orders; without → all
orders (admin).
```json
→ {"command": "get_orders", "restaurant_id": 4}

← {"type": "response", "command": "get_orders",
   "data": [{"id": 3, "restaurant_id": 4, "customer_id": 2,
             "status": "pending", "total_price": 410000,
             "created_at": "2026-11-02 18:53:49"}]}
```

### `get_orders_for_restaurant`
Restaurant-scoped variant that also resolves the customer username:
```json
→ {"command": "get_orders_for_restaurant", "restaurant_id": 4}

← {"type": "response", "command": "get_orders_for_restaurant", "success": true,
   "data": [{"id": 3, "customer_username": "demo_customer", "status": "pending",
             "total_price": 410000, "created_at": "2026-11-02 18:53:49"}]}
```

### `update_order_status`
```json
→ {"command": "update_order_status", "order_id": 3, "new_status": "confirmed"}

← {"type": "response", "command": "update_order_status", "success": true}

⟳ push to all other clients:
{"type": "order_status_update_notification",
 "data": {"order_id": 3, "new_status": "confirmed"}}
```
Invalid status values are rejected by the database layer.

## 4. Chat

### `send_chat_message`
No direct reply — the message is **broadcast to every other connected
client**, which filter by `order_id` in the UI.
```json
→ {"command": "send_chat_message", "order_id": 3,
   "sender": "پیتزا پاردیس", "message_text": "سلام، سفارش آماده است"}

⟳ push to all other clients:
{"type": "new_chat_message_notification",
 "data": {"command": "send_chat_message", "order_id": 3,
          "sender": "پیتزا پاردیس", "message_text": "سلام، سفارش آماده است"}}
```

## 5. Reviews

### `submit_review`
One review per order (UNIQUE constraint). `rating` is clamped to 1–5;
the server resolves `restaurant_id` / `customer_id` from the order.
```json
→ {"command": "submit_review", "order_id": 3, "rating": 4,
   "review_text": "سریع بود"}

← {"type": "response", "command": "submit_review", "success": true}
```

### `get_reviews`
```json
→ {"command": "get_reviews"}

← {"type": "response", "command": "get_reviews",
   "data": [{"order_id": 3, "restaurant_id": 4,
             "restaurant_name": "پیتزا پاردیس",
             "customer_name": "demo_customer", "rating": 4,
             "comment": "سریع بود", "created_at": "2026-11-02 18:59:44"}]}
```

## 6. Profiles & administration

### `update_profile`
```json
→ {"command": "update_profile", "user_id": 2,
   "data": {"username": "demo_customer", "address": "تهران"}}

← {"type": "response", "command": "update_profile", "success": true}
```
### `get_users`
```json
→ {"command": "get_users"}

← {"type": "response", "command": "get_users",
   "data": [{"id": 2, "username": "demo_customer", "role": "customer",
             "is_active": true, "is_approved": true}]}
```

### `block_user` / `unblock_user` / `delete_user`
```json
→ {"command": "block_user", "user_id": 2}
← {"type": "response", "command": "block_user", "success": true}
```

### `approve_restaurant` / `disapprove_restaurant`
Toggles `is_approved` on a `role = 'restaurant'` account; unapproved
restaurants never appear in `get_restaurants`.
```json
→ {"command": "approve_restaurant", "user_id": 5}
← {"type": "response", "command": "approve_restaurant", "success": true}
```

## 7. Wallet (v2)

Every user carries a `balance`. All mutations go through transactional
SQL, so concurrent payments can never overdraw a wallet.

### `get_wallet_balance`
```json
→ {"command": "get_wallet_balance", "user_id": 2}
← {"type": "response", "command": "get_wallet_balance", "balance": 200000}
```

### `charge_wallet`
```json
→ {"command": "charge_wallet", "user_id": 2, "amount": 150000,
   "note": "شارژ خوش‌آمدگویی"}
← {"type": "response", "command": "charge_wallet", "success": true}
```

### `pay_from_wallet`
Fails when the balance is below `amount`.
```json
→ {"command": "pay_from_wallet", "user_id": 2, "amount": 180000, "order_id": 4}
← {"type": "response", "command": "pay_from_wallet", "success": true}
```

## 8. Coupons (v2)

Coupon kinds: `"percent"` (value = percent, ≤ 100) and `"amount"`
(value = تومان). Constraints enforced by the server: `min_order`,
`max_uses` (`0` = unlimited), `expires_at` (`""` = never), `is_active`.

### `create_coupon` (admin)
```json
→ {"command": "create_coupon", "code": "WELCOME20", "kind": "percent",
   "value": 20, "min_order": 100000, "max_uses": 100, "expires_at": ""}
← {"type": "response", "command": "create_coupon", "success": true}
```

### `get_coupons`
```json
→ {"command": "get_coupons"}
← {"type": "response", "command": "get_coupons",
   "data": [{"id": 1, "code": "WELCOME20", "kind": "percent", "value": 20,
             "min_order": 100000, "max_uses": 100, "used_count": 3,
             "is_active": true, "expires_at": ""}]}
```

### `toggle_coupon` (admin)
```json
→ {"command": "toggle_coupon", "coupon_id": 1, "active": false}
← {"type": "response", "command": "toggle_coupon", "success": true}
```

### `validate_coupon`
Read-only check used by the checkout dialog; answers Persian error text
in `message` when the code is unknown, inactive, expired, exhausted or
below `min_order`.
```json
→ {"command": "validate_coupon", "code": "welcome20", "total": 410000}
← {"type": "response", "command": "validate_coupon", "ok": true,
   "discount": 82000, "coupon_id": 1, "message": "کد تخفیف اعمال شد 🎉"}
```
Codes are case-insensitive (normalized to upper-case on both sides).

## 9. Errors

Unknown commands answer immediately:
```json
→ {"command": "does_not_exist"}
← {"type": "response", "command": "does_not_exist", "error": "Unknown command"}
```
Malformed lines (not JSON, no `"command"`) are dropped with a log entry.

Authentication/authorization failures (v2.2) answer with
`success: false` plus an `error` discriminator:

| `error` | Meaning | Typical cause |
|---|---|---|
| `unauthorized` | no valid session | token missing, forged or expired; also after `logout` |
| `forbidden` | session lacks role/ownership | customer calling an admin command, touching a foreign wallet, editing another profile… |

## 10. Connection guards (v2.1)

The server protects itself per TCP connection:

- **Rate limiting** — each connection gets a token bucket of 60 requests with a
  25 req/s refill. A connection that exhausts the bucket receives one explanation
  and is disconnected:
```json
← {"type": "rate_limit_error", "message": "Too many requests"}
```
  The limit is per connection, so 50 concurrent clients each get their own budget.
- **Message size cap** — a single frame longer than 64 KiB (with or without the
  terminating newline) is dropped and the connection is closed:
```json
← {"type": "rate_limit_error", "message": "Message too large"}
```
- **Login cost** — every `login` performs one PBKDF2-HMAC-SHA256 derivation
  (30 000 iterations) on the server; clients should treat login as a slow-path
  command (~30 ms) and keep other requests on the fast path. Passwords created
  before v2.1 are migrated from unsalted SHA-256 to PBKDF2 automatically on the
  first successful login.

## 11. Session guards (v2.2)

- **Token issuance** — only a successful `login` obtains a token; there is no
  other way onto the server. Tokens are 256-bit values from the operating
  system CSPRNG (`QRandomGenerator::system()`), so they cannot be guessed or
  forged by cycling connection ids.
- **Idle expiry** — a session dies after 30 minutes without traffic (sliding
  window, refreshed by every accepted request). The server sweeps expired
  sessions once a minute and caps the store at 512 live sessions, so login
  floods cannot grow memory without bound.
- **Role matrix** — the dispatch table enforces the §1 authorization model on
  every command; a customer session simply cannot reach `block_user`, and a
  restaurant session cannot read another restaurant's orders.
- **Identity stamping** — pushed `new_order_notification`s carry the
  username bound to the session, so restaurant dashboards cannot be fed a
  spoofed customer name.

## 12. Transport security — TLS (v2.3)

The framing is byte-identical with and without TLS: the JSON lines of
this document are simply wrapped inside a standard TLS 1.2/1.3 channel
instead of raw TCP.  The server listener stays on the **same port**;
when it runs with a certificate it accepts *only* TLS connections (a
plain-text client is disconnected during the handshake, never answered
in the clear).

### Enabling it (server)

```bash
iut-food-server --tls-cert certs/server.crt --tls-key certs/server.key
```

- Both flags together or neither; without them the listener is plain TCP.
- The certificate must be PEM; the key can be RSA or EC, unencrypted
  (`QSslKey` parses both).
- No client certificates are requested — the server authenticates
  clients through the §1 session tokens, not through mutual TLS.
- `get_server_stats` reports how many connections completed the
  handshake under `tls_connections`.

### Certificates for a demo

`scripts/generate_certs.sh` creates a self-signed pair
(`certs/server.crt` + `certs/server.key`) with
`subjectAltName=DNS:localhost,IP:127.0.0.1` — exactly what modern TLS
stacks require for a loopback demo.  For real deployments use a
CA-signed certificate and keep the key out of any shared folder.

### Client side

The login dialog has a connection section: server address, port, a
**رمزگذاری TLS** switch and a **پذیرش گواهی self-signed** consent
checkbox (off = the handshake aborts and the certificate error is
shown).  Both switches persist via `QSettings`; the JSON wire format,
session tokens and the smoke suite (`scripts/smoke_test.py --tls`) are
identical to plain mode.

### In the test suite

`tests/tls_tests.cpp` boots a real server in-process with the bundled
`tests/certs/*.pem` fixtures and verifies: the handshake presents the
expected certificate, login + wallet round-trips stay encrypted, a chat
broadcast reaches a second TLS client, and a plain-text client is
rejected.  The suite skips itself when the Qt build has no TLS backend.

## 13. Server logging (v2.4)

The server mirrors everything it logs to stderr **and** (since v2.4) to an
optional size-rotated file, so a long-running deployment can be inspected
after the fact:

```
iut-food-server --log-file logs/server.log --log-level info
```

| Flag | Meaning |
|---|---|
| `--log-file <path>` | append to this file; rotated at 5 MiB into `<path>.1`, `<path>.2`, `<path>.3` (3 generations); omitted = stderr only |
| `--log-level <level>` | minimum severity: `debug` < `info` < `warning` < `error`; a typo falls back to `info` |

Line shape (all lines carry a millisecond timestamp and a component tag):

```
2026-11-11 19:12:44.312 [info] server: listening on port 1234 (TLS enabled) | database: /data/iut_food.db
2026-11-11 19:12:51.880 [debug] req: ::ffff:127.0.0.1 get_menu_items_for_restaurant 1ms
2026-11-11 19:13:02.115 [warning] guard: rejecting client ::ffff:10.0.0.7: Message too large
2026-11-11 19:14:00.000 [info] tls: handshake complete: ::ffff:127.0.0.1 | TLS_AES_256_GCM_SHA384
```

Levels are assigned so the file stays readable at the default `info`:
per-request round trips (with durations) are `debug`, connection lifecycle
and TLS handshakes are `info`, rate-limit / frame-cap rejections are
`warning`, and start-up failures are `error`.

### Graceful shutdown

`SIGTERM` / `SIGINT` / Ctrl+C / `docker stop` are handled as a shutdown
request: the listener closes, every connected client receives a final
`{"type":"server_shutdown"}` notification before its socket closes, and the
process exits with code 0.  Clients treat the note like any other
notification (unknown types are ignored), so older clients simply see the
connection end.
