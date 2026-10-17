# Changelog

All notable changes to IUT-Food are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/) (admittedly loosely).

## [2.4.0] — 2026-11-11

The "performance & observability" release: the data layer gets the standard
SQLite tuning for a long-running service, and the server finally ships a
real logger plus a graceful shutdown path.

### Added
- 📜 **Rotating file logger** (`src/common/logger.h`) — `--log-file` mirrors every
  line into a size-rotated file (5 MiB per generation, 3 kept as
  `server.log.1`, `server.log.2`); `--log-level` filters by severity
  (debug / info / warning / error, a typo falls back to info).  Per-request
  round trips are logged as `debug` with their duration; rejections as
  `warning`.
- 👋 **Graceful shutdown** — SIGTERM / SIGINT / Ctrl+C / `docker stop` close the
  listener, send every connected client a final `{"type":"server_shutdown"}`
  notification, flush, and exit 0.  POSIX signals are handled through an
  async-signal-safe atomic flag polled by the event loop; Windows uses
  `SetConsoleCtrlHandler`.
- 🧪 **`logger_tests` suite (4 cases)** — level filtering, rotation generations,
  stderr-only fallback and the level parser.  Nine suites / 52 cases total.
- 🧪 **WAL regression check** — `orders_tests` asserts every freshly opened
  database reports `journal_mode = wal` and `synchronous = 1`.
- 🐳 **Compose log volume** — both demo services now write
  `/data/logs/server.log` inside the existing data volume.
- 📊 **Benchmark `--pacing` flag** — sustained runs can stay deliberately under
  the 25 req/s token-bucket refill (a 50×200 unpaced run trips the limiter by
  design); `--label` annotates the results JSON.

### Changed
- 🚄 **SQLite performance profile** — every opened database now runs with
  `journal_mode = WAL`, `synchronous = NORMAL` and a 5 s busy timeout, so
  readers never block writers and rare contention waits instead of failing.
- 🚄 **Prepared-statement reuse** — the hot read paths (login lookup,
  restaurant listing, menu, wallet balance, order lists, wallet history) reuse
  compiled statements instead of re-preparing on every request.
- 📉 **Measured effect** (same 2-core sandbox, same workload): throughput stays
  at the event-loop ceiling (~400 req/s burst, 632 req/s paced) while
  **p50 drops 1.70 → 1.06 ms and p99 3.52 → 2.78 ms** in the burst run, and
  p50 0.83 → 0.37 ms / p99 2.43 → 1.75 ms in the paced 10,000-request run.
  The baseline numbers are kept in `docs/benchmark-results-baseline.json` and
  the chart now draws both releases side by side.
- 🧾 `docs/PROTOCOL.md` gains §13 (logging + graceful shutdown).

## [2.3.0] — 2026-11-10

The "encrypted transport" release: the same newline-delimited JSON protocol can
now travel inside a standard TLS channel, end to end — server flags, client
switch, tests and CI included.

### Added
- 🔒 **TLS transport (opt-in)** — start the server with
  `--tls-cert certs/server.crt --tls-key certs/server.key` and every connection
  performs a standard TLS 1.2/1.3 handshake (QSslSocket +
  `startServerEncryption`); the framing stays byte-identical and the listener
  keeps the same port. Plain-text clients are disconnected during the
  handshake, never answered in the clear.
- 🧾 **Certificate loading** — `Server::enableTls()` parses PEM chains and
  RSA/EC keys, logs parse failures and keeps the server in plain mode instead
  of starting half-configured.
- 🖥 **Client connection settings** — the login dialog gained a connection
  section (server address, port, **رمزگذاری TLS** switch, self-signed consent
  checkbox); all four values persist via `QSettings`, and the network manager
  no longer forgets a custom host/port when it re-establishes a session.
- 🧪 **`tls_tests` suite (4 cases)** — boots a real server in-process with the
  bundled `tests/certs/*.pem` fixtures (CN=localhost, SAN 127.0.0.1):
  handshake + certificate identity, encrypted login + wallet round-trip, a
  chat broadcast between two TLS clients, and rejection of a plain-text
  client. Skips itself when the Qt build has no TLS backend.
- 🐳 **TLS demo stack** — `docker-compose.yml` grew a `server-tls` service
  (profile `tls`) that mounts `./certs`; `scripts/generate_certs.sh` creates a
  self-signed SAN certificate in one line.
- 📊 **`tls_connections` counter** — `get_server_stats` now reports how many
  connections completed the handshake.

### Changed
- CI: the smoke suite runs **a second time over TLS** (`--tls --port 1235`)
  on the Ubuntu job, and the `main`-branch push trigger is fixed
  (a corrupted `branches` filter previously silenced it).
- The server code is built as a linkable `iutfood_server` library so the TLS
  suite can boot it in-process; the executable target shrinks to `main.cpp`.
- `scripts/smoke_test.py` accepts `--tls` and `--port` and checks the
  negotiated protocol version and cipher when run encrypted.
- Unit test count grows from 7 suites / 43 cases to **8 suites / 47 cases**.

## [2.2.0] — 2026-11-08

The "secure sessions" release: the wire protocol gets real authentication,
the admin console gets a gate, and the project gets Windows builds plus
an automated release pipeline.

### Added
- 🔐 **Session authentication (wire protocol)** — a successful `login` now mints a
  256-bit session token (OS CSPRNG, hex-encoded) with a 30-minute sliding idle
  lifetime. Every other command must present it; sessions support `logout`,
  sweeping, and a 512-session store cap.
- 🛡 **Role & ownership enforcement** — the server no longer trusts client-sent
  ids: wallets, profiles, orders and menus are checked against the session
  identity. Admin-only management (`get_users`, `block_user`, coupon manager…),
  restaurant-only menu/order commands and customer self-scoping are enforced in
  the dispatcher; violations answer `unauthorized` / `forbidden`.
- 🧾 **`get_server_stats`** (admin) — uptime, active sessions, connections,
  requests served/rejected and auth failures, straight from the running server.
- 🪟 **Windows build in CI** — a `windows-2022` matrix job runs the full pipeline
  (CMake → Qt Test → smoke suite) on every push, next to the Ubuntu job.
- 📦 **Automated release pipeline** (`release.yml`) — every `v*` tag now
  attaches a portable Windows x64 build (Qt DLLs bundled via `windeployqt`) and
  a Linux tarball to the GitHub release, each with a SHA-256 checksum; both are
  smoke-tested before upload.
- 🔑 **Admin console sign-in** — the desktop admin panel asks for credentials
  (new `Admin` login mode) instead of opening unauthenticated.

### Changed
- The GUI clients open the authenticated TCP session right after the local
  sign-in succeeds; `NetworkManager` attaches the token to every outgoing
  request and parks requests that fire before the login round-trip finishes.
- `new_order_notification`s are stamped with the username bound to the
  session, so restaurant dashboards cannot be fed spoofed customer names.
- Smoke suite grown from 35 to 52 checks, including a dedicated
  authentication/authorization section (forged tokens, foreign wallets,
  role matrix, logout revocation).

### Security
- Previously **every** command — including user deletion, wallet charging and
  user listing — was callable without any credentials. v2.2 closes this by
  requiring a server-issued session for everything except `login` itself.

## [2.1.0] — 2026-10-31

The "production-ready" release: tested, hardened, containerised and measured.

### Added
- 🧪 **Six Qt Test suites (33 cases)** — the PBKDF2 primitive is checked against the
  official RFC vectors; wallet atomicity, coupon rules, the canonical order-state
  machine, rate limiting and the legacy-hash migration all run against scratch
  databases. Wired into CI with `ctest`.
- 🐳 **Docker** — a two-stage `Dockerfile` (distro Qt 6 toolchain → slim runtime) plus
  `docker-compose.yml` for a one-command demo server; CI builds the image and
  smoke-tests the running container on every push.
- 📏 **Benchmark harness** (`scripts/benchmark.py`) — 50 concurrent clients with
  per-request latency recording; the real results (394 req/s, p50 1.6 ms, p99 3.2 ms)
  are published in the README with a chart.

### Security
- 🔑 **Salted password hashing** — passwords are now stored as
  `pbkdf2_sha256$<iterations>$<salt>$<hash>` (PBKDF2-HMAC-SHA256, 30 000 iterations,
  16-byte random salt, constant-time comparison), implemented on Qt's
  `QMessageAuthenticationCode` so the project stays dependency-free.
- 🔁 **Transparent migration** — pre-v2.1 databases keep working: on the first
  successful login, a legacy unsalted SHA-256 digest is re-hashed to PBKDF2 in place.
- 🚦 **Per-connection rate limiting** — a token bucket (60-request burst, 25 req/s
  refill) sheds floods; abusive connections receive `rate_limit_error` and are closed.
- 📏 **Message size cap** — frames larger than 64 KiB are rejected and the connection
  is closed before the read buffer can grow without bound.

### Fixed
- `updateOrderStatus` now reports failure for unknown/deleted order ids instead of
  silently succeeding (caught by the new unit tests).

## [2.0.0] — 2026-10-25

The "real product" release: money, discounts, live tracking and a dark mode.

### Added
- 💳 **Wallet** — per-user balance with a transactional top-up/payment API
  (`payFromWallet` deduces atomically and can never overdraw), a wallet page in the
  profile (balance, top-up, transaction log) and wallet payment at checkout with a
  live balance check that disables the pay button when funds are short.
- 🎟 **Coupon codes** — `coupons` table with percent/fixed kinds, minimum order,
  usage limit and expiry; server-side validation + consumption tracking; admin
  manager tab (create / toggle) and a checkout box that validates live and shows
  the discount.
- 🚦 **Live order tracking** — non-modal pipeline window
  (`placed → confirmed → shipped → delivered`, plus a cancelled state) that updates
  in place from the server's status pushes instead of interrupting modal dialogs.
- ⭐ **Favorites** — bookmark restaurants from the menu dialog and filter the
  restaurant list down to them.
- 🔄 **Reorder** — one button pushes a previous order's dishes back into the cart.
- 🌙 **Dark / light themes** — full stylesheets applied application-wide, toggled
  from the launcher, the customer toolbar and the admin menu bar, persisted in
  `QSettings`; the admin console no longer carries a hard-coded stylesheet.
- Protocol: `get_wallet_balance`, `charge_wallet`, `pay_from_wallet`, `get_coupons`,
  `create_coupon`, `toggle_coupon`, `validate_coupon` and the
  `notify_order_placed` broadcast (`new_order_notification`).
- `scripts/smoke_test.py` grew from 22 to 35 checks (wallet, coupons, consumption,
  overdraw rejection, inactive coupons, new-order broadcast).
- Schema v3 with in-place migration: `users.balance`, `orders.discount`,
  `orders.coupon_code`, `orders.payment_method`, plus `wallet_transactions`,
  `coupons`, `favorites` tables — old v1/v2 databases upgrade automatically.
- `scripts/screenshot_driver.cpp` — offscreen capture driver that produces the
  documentation screenshots from the real windows.
- CONTRIBUTING.md, SECURITY.md, GitHub issue templates, release tags.

### Fixed
- **Duplicate orders**: going online while checking out re-inserted the order
  through `place_order` after it was already written locally. The client now
  broadcasts `notify_order_placed` (informational) instead of re-inserting.
- **Server and client used different SQLite files**: the server defaulted to
  `server.db` inside its working directory while the client opened
  `iut_food.db` next to its binary, so a same-machine demo silently split
  its data. Both now default to `iut_food.db` next to the binary
  (`--db` still overrides for split deployments).

### Changed
- Wallet charge + coupon consumption happen only after the order and its items
  are safely persisted; a failed wallet payment cancels the order it created.
- Demo seed now includes a pre-charged wallet (200,000 تومان), two demo coupons
  (`WELCOME20`, `IUT50`) and a bookmarked restaurant.
- Smoke-test coverage and documentation screenshots regenerated for v2.

## [1.0.0] — 2026-10-18

The first release that actually compiles, runs, and is safe to demo.
This is a complete overhaul of the original coursework tree.

### Fixed — correctness
- **Server did not compile**: `DataBaseHandler` was instantiated through a
  private constructor and includes used wrong file casing.
- **Customer requests never reached the server**: `NetworkManager::sendJson`
  forgot the `\n` framing the server parses with `canReadLine()`.
- **Admin panel could never connect** (port 5555 vs. server 1234) and used
  its network manager through an uninitialised pointer.
- **Order items were never persisted**: queries referenced a
  `menu_item_id` column that does not exist (`food_id` is the real one);
  the checkout path also inserted them twice.
- **Three different database files** were opened depending on which
  launcher button you clicked — now one `iut_food.db` per app.
- **The customer menu was always empty**: `MenuDialog` read the dead
  `menu_items` table; it now reads `foods`, the table actually written to.
- **Order status machine was broken**: the client stored Persian text, the
  restaurant dashboard compared against English literals. All states are
  now canonical (`pending/confirmed/shipped/delivered/cancelled`) with a
  Persian display mapping (`orderstatus.h`).
- **Every order belonged to "user 1"**: there was no login at all; the new
  login/registration dialog gives each customer a real session.
- **Profile screen listed all users' orders**; it is now scoped to the
  signed-in customer and items appear once (the nested debug loop is gone).
- **Admin selection operated on `user_id = 0`**: the table never stored the
  id in `Qt::UserRole`; block/delete/approve now hit the right row.
- **Admin charts showed hard-coded demo numbers**; all four reports are
  computed from live server data (plus a new ratings report).
- Restaurant dashboard "Created At" column was never filled; leak on every
  launcher click fixed; cart is emptied after checkout; duplicate
  `get_restaurants` requests and double-connected signals removed.

### Changed
- New **`LoginDialog`** (customer / restaurant modes) with registration.
- `NetworkManager` and `AdminNetworkManager` merged into one correct
  singleton; the server pushes `order_status_update_notification`.
- Server gained `--port` / `--db` flags, real broadcasts (chat + status),
  and a `get_reviews` command; the unused Qt-Creator `mainwindow` template
  and the dead `hasOrder` chat path were removed.
- Demo data is seeded automatically on first launch (restaurants, menus,
  orders, one review) — every screen has content out of the box.
- Food names are snapshotted into `order_items`, so order history survives
  menu edits and deletions.
- Passwords stored as SHA-256 hashes (see README roadmap for the caveat).
- Prepared statements everywhere (the restaurant search had string-built SQL).
- Bundled **Vazirmatn** font + RTL layout; all screens share one visual theme.
- All images converted to **WebP** (1.04 MB hero → 121 KB) and actually
  shipped via the resource file (the old `resourses.qrc` referenced three
  files that did not exist and was never compiled into any binary).

### Added
- Live **chat tab** in the restaurant dashboard.
- Customer-side **delivery confirmation** and star-rating flow.
- `scripts/smoke_test.py` — 22-check end-to-end protocol test.
- CMake build (Qt 5.15+/Qt 6), qmake retained, GitHub Actions CI.
- Documentation: architecture diagrams, `docs/PROTOCOL.md`, screenshots.

### Removed
- Dead code: broken `order.cpp`, six unused `NetworkManager` request
  methods, empty `adminpanel.ui` / `profilepanel.ui` placeholders,
  debug database-status helpers, commented-out blocks.
