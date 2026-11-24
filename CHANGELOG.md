# Changelog

All notable changes to IUT-Food are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/) (admittedly loosely).

## [2.9.0] — 2026-11-23

The "everywhere" release: the same sources now ship as native
artifacts for four platforms, and the client runs on sandboxed
platforms without touching its desktop behaviour.

### Added
- 🤖 **Android APK (arm64-v8a)** — the release workflow cross-builds
  the client with the Qt 6.8 Android kit against the NDK and attaches
  a debug-signed, directly-installable APK to every tag
  (`docs/ANDROID.md` covers install, server connection and
  troubleshooting).  Launcher icons in five densities, app label and
  package id (`ir.iut.food`) come from the new
  `packaging/android/res` tree.
- 🐧 **Self-contained Debian package** — `scripts/package_deb.sh`
  stages the client + server with their Qt libraries, the xcb/eglfs/
  offscreen platform plugins, the WebP image formats and the SQLite
  driver under `/opt/iut-food` (`$ORIGIN` rpaths), adds a `.desktop`
  entry and an hicolor icon, and builds `iut-food_<v>_amd64.deb` —
  no system Qt required on the target machine.
- 🍎 **macOS DMG** — a new release job builds the client as an .app
  bundle and deploys it with `macdeployqt -dmg`.
- 📇 **`CITATION.cff`** — the repo is now citable; the GitHub
  "Cite this repository" button picks it up.

### Changed
- 📁 **Platform-aware database location** — the client kept its
  database next to the binary, which is read-only on Android (inside
  the APK) and inside the bundle on macOS.  The new `AppDirs` helper
  moves it to `QStandardPaths::AppDataLocation` on those two
  platforms; Windows and Linux keep the portable
  *database-next-to-binary* behaviour (and `run_demo.sh` keeps
  working unchanged).
- 📦 **Release matrix** — `release.yml` now produces windows-x64.zip,
  linux-x64.tar.gz + amd64.deb, macos.dmg and android-arm64.apk,
  each with SHA-256 checksums, plus a `workflow_dispatch` trigger for
  manual re-runs.
- 🎨 **One brand mark everywhere** — a proper app icon (a white serving
  cloche on the stylesheet-blue tile) is compiled into the Windows
  executables (`.ico` resource), bundled into the macOS .app
  (`.icns`), installed by the deb, shown in the window/taskbar on
  every desktop OS and used for the Android launcher in all five
  densities; regenerate the whole set with
  `scripts/make_brand_assets.py`.
- 🗂 **Packages that explain themselves** — every release archive
  extracts into one folder: the two executables (double-click
  launchers on Windows) on top, `docs/` and `licenses/` beside them
  and a bilingual `START-HERE.txt` quickstart (English + فارسی);
  the Linux tarball adds an optional `desktop/install-desktop.sh`
  menu-entry installer, and the Windows zip exposes only the two
  launchers with the Qt runtime tucked into `runtime/`.

### Fixed (carried over from the Nov 20 push)
- 🔒 **TLS by default on the client** — new connections are encrypted
  unless explicitly switched off; the self-signed waiver is scoped to
  the host you actually consented to (the server keeps printing the
  cleartext warning when it runs without certificates).
- 🐳 **Docker hardening** — the server container runs as a dedicated
  non-root user (UID 1001; 24.04 images reserve 1000).
- 📚 **`docs/adr/0001`** — the opt-in-TLS + offline-first design is
  recorded as an architecture decision record.

## [2.8.0] — 2026-11-18

The "presentable" release: one command runs the whole product, the
client finishes the v2.7 version handshake on screen, and the desktop
UX gets the polish a reviewer expects.

### Added
- 🎬 **One-command demo** (`scripts/run_demo.sh`) — builds when needed,
  parks your database aside, lets the server seed a fresh one, waits
  for the built-in `--healthcheck` probe and launches the client;
  `--tls`, `--metrics` and `--no-client` variants; closing the client
  restores your data.  The click-by-click tour lives in the new
  `docs/DEMO.md`, and `scripts/make_demo_gif.py` stitches the
  screenshot gallery into the README's `docs/images/demo.gif`.
- 🪟 **Permanent connection & server-version status bar** — the
  customer workspace pins two labels instead of flashing 4-second
  messages: the link state (with a `🔒TLS` badge on encrypted
  sessions) and the release the server announced in its login reply.
- 🖼 **`docs/ARCHITECTURE.md`** — module map, the life of a request,
  the security-model table and the threading model in one document.

### Changed
- 🤝 **The client listens to the version handshake** — since v2.7 every
  login reply carries `server_version`; the client kept dropping the
  field.  `NetworkManager::serverVersion()` plus two new signals
  surface it in the status bar, and the server's `outdated_client`
  warning finally reaches the user as a 12-second status message.
- ⌨️ **Desktop-standard shortcuts & hints** — `F1` opens About (which
  now names the MIT license and the repository), `Ctrl+Q` quits,
  `Ctrl+F` jumps into the restaurant search (which gained a clear
  button), tooltips explain every launcher button and toolbar action,
  and window titles carry the running release.

### Fixed
- 🧯 **Flaky snapshot collisions on coarse clocks** — two rapid
  snapshots could land on the same millisecond stamp, and `VACUUM INTO`
  refuses to overwrite an existing file (it made the backup retention
  test fail intermittently under ctest).  Destinations are now
  uniquified with zero-padded suffixes, keeping lexical order ==
  chronological order for pruning.
- 📖 **README test table repeated the three v2.7 suites three times** —
  one row per suite again; the clone instructions also say
  `cd IUT_Food` (the directory git actually creates).

## [2.7.0] — 2026-11-18

The "observable & operational" release: the server joins standard
monitoring stacks (Prometheus, Docker health), the admin console
exports its reports, and CI grows an ASan+UBSan leg.

### Added
- 📊 **Prometheus `/metrics` endpoint** (`src/server/metricsserver.h`) —
  start the server with `--metrics-port 9101` and any Prometheus
  scraper can read nine families in the text exposition format
  (build info with version label, uptime, active/total sessions,
  accepted/TLS connections, served/rejected requests, auth failures).
  Read-only, one-shot HTTP/1.0, plain HTTP by design (scraping belongs
  on the internal network); 404 for unknown paths, 405 for non-GET
  methods.  The same counters power `get_server_stats`, so the admin
  tab and a Grafana panel can never disagree.
- 🤝 **Version handshake on the wire** — a new unauthenticated `ping`
  command answers `pong` with `server_version` and `server_time`, and
  every login reply names the release; an optional `client_version`
  field earns `"warning": "outdated_client"` when the client's major
  version is behind (purely additive: older clients are unaffected).
- 🩺 **`--healthcheck` probe + Docker `HEALTHCHECK`** — the server
  binary can probe a running instance (send `ping`, expect a pong with
  a version) and exit 0/1, so the Dockerfile ships a real
  `HEALTHCHECK` and `docker compose` services go green in `docker ps`.
  The probe runs before any database is opened, so it never touches
  the SQLite file of the process it checks.
- 🧾 **CSV export in the admin console** — new "⬇️ خروجی CSV" buttons on
  the reports tab and the order browser.  The CSV writer implements
  RFC 4180 quoting and prepends a UTF-8 BOM so Persian headers survive
  Excel on Windows (a test pins the exact quoting of
  `فست فود "قهرمان"` and `9,700`).
- 🧪 **Three new test suites** — `metrics_tests` (text format + real
  HTTP round trips), `csv_tests` (quoting, BOM, Persian) and
  `handshake_tests` (ping, version fields, outdated-client warning
  against a real in-process server).  Fourteen suites, 73 cases.  The
  protocol smoke test grew eight v2.7 checks.
- 🧵 **`linux-asan` preset + CI sanitizers job** — the whole suite
  compiles with `-fsanitize=address,undefined -fno-sanitize-recover=all`
  (`cmake --preset linux-asan`) and runs green in a new CI job.
  Leak detection stays off in the preset: Qt keeps intentional
  process-lifetime singletons LeakSanitizer would misreport.
- 🗂 **Issue & PR templates** — GitHub issue forms for bugs (component,
  steps, platform, version) and feature requests, plus a PR checklist.

### Fixed
- 🏷 **`iut-food-server --version` lied** — it still printed `2.5.0`
  while v2.6's changelog claimed a single version header; it now reads
  `IUT_FOOD_VERSION` like everything else.
- 🧯 **Qt 6's UTF-8 decoder eats the BOM** — `QString::fromUtf8` treats
  a leading U+FEFF as encoding metadata and returns an empty string;
  the CSV writer therefore emits the byte-order mark as an explicit
  `QChar(0xFEFF)` (documented in `csvwriter.h`, pinned by a test that
  would otherwise have passed vacuously with an empty needle).

## [2.6.0] — 2026-11-15

The "presentable product" release: everything a reviewer pokes before
trusting a repo now matches what the code actually does, plus a live
operations view inside the admin console.

### Added
- 🖥 **Live server-health tab in the admin console** — a new "سلامت سرور"
  tab calls the admin-only `get_server_stats` command and shows uptime,
  active/total sessions, accepted connections, TLS connections, served
  and rate-limited requests and auth failures; it refreshes every time
  the tab is opened or the refresh button is pressed.  (The command
  existed since v2.2 — no client UI ever called it, while the README
  already advertised "live server-health stats".)
- 🚩 **`--no-demo-data` server flag** — real deployments can now start a
  fresh database empty instead of silently seeding `admin/admin123`,
  demo restaurants, pre-charged wallets and coupon codes.
- ℹ️ **About dialog in the client launcher** — the GUI finally says which
  release it is (the server had `--version` from the start).
- 🧭 **CMakePresets.json** — `cmake --preset linux-release` /
  `windows-release` with matching build and ctest presets.
- 🎨 **`.clang-format` + `.clang-tidy`** — the code style that used to
  live only in CONTRIBUTING prose is now a machine-readable baseline
  (`clang-format -i` on touched files; the linter stays advisory).
- 🧪 **Backup failure-path regression test** — a snapshot into an
  unwritable destination must fail with a truthful message and must not
  leak its named SQLite connection (see the fix below).  The blocked
  destination is built portably (a regular file where the backup
  directory belongs): the first cut chmod'ed a directory read-only,
  which Windows ignores for folders, so the suite failed on
  windows-2022 while Ubuntu stayed green.  Unit suite count: eleven
  suites, 60 cases.
- 📸 **Screenshot pipeline** — the driver captures a thirteenth window
  (the server-health tab) and `scripts/convert_screenshots.py` documents
  the PNG→WebP step that used to be tribal knowledge; three orphaned
  screenshots from an older numbering scheme were dropped.

### Fixed
- 🧯 **BackupManager failure paths** — open()/`VACUUM INTO` failures
  returned early and left a dead named connection in Qt's global
  registry per failed snapshot (unbounded growth on an unwritable
  backup directory), and `backupFinished(false, …)` emitted an **empty**
  message because `m_lastError` was never written.  Both fixed; the new
  regression test pins the behaviour.

### Changed
- 🔐 **CI actually exercises TLS now** — the Ubuntu leg runs the full
  smoke suite a second time against a `--tls-cert/--tls-key` server
  (README claimed this since v2.3; now it is true, and the push branch
  filter of the workflow was double-checked while in there).
- 🧾 **SECURITY.md rewritten** — the old policy still claimed "no TLS,
  unsalted SHA-256, no session auth", i.e. it contradicted v2.1–v2.3.
  It now carries the real supported-version matrix, the actual security
  posture with pointers to the tests, and the honest remaining
  limitation (clear-text credentials without TLS).
- 📚 **CONTRIBUTING.md refreshed** — presets-based quick start, the
  screenshot regeneration recipe, the one-version rule, and metric
  numbers that no longer drift ("50+ checks" instead of a hardcoded
  count from v2.0).
- 🏷 **One version to rule them all** — `src/common/version.h` is the
  single source of truth: CMake parses it into `project(VERSION)`, the
  server `--version`, the client About dialog and the screenshot driver
  all read the same macro (the driver still claimed 2.0.0 in v2.5).

## [2.5.0] — 2026-11-14

The "operability" release: the server can now back itself up on a
schedule and answers "who did what" with a machine-readable audit trail.

### Added
- 💾 **Rolling SQLite snapshots** (`src/common/backupmanager.h`) — `--backup-dir`
  enables `VACUUM INTO` snapshots, which stay valid, fully compacted
  databases even while orders are being written in WAL mode;
  `--backup-interval` (default 60 min) paces them and `--backup-keep`
  (default 5) bounds the directory.  A baseline snapshot runs at start-up
  and a final one during the graceful shutdown, so `docker stop` never
  loses more than a minute of data; a failed snapshot logs a warning and
  never stops the server.
- 🧾 **JSONL audit trail** (`src/common/auditlog.h`) — `--audit-file` appends
  one compact JSON object per security-relevant event: `login_ok` /
  `login_failed`, `logout`, `moderation` (block / unblock / delete /
  approve), `coupon_created` / `coupon_toggled`, `wallet_topup` /
  `wallet_paid` and `order_placed` — each with actor, peer address and
  structured amounts or ids.  Rotated at 5 MiB, 3 generations kept; one
  event per line means `jq` is already a working security report.
- 🐳 **Compose side channels** — the demo services now write snapshots and
  the audit trail into the existing data volume (`/data/backups`,
  `/data/audit/audit.jsonl`), so they survive container recreation.
- 🧪 **`backup_tests` (4 cases) + `audit_tests` (3 cases)** — snapshot
  validity against a second connection, retention pruning, disabled and
  missing-source error modes, JSONL shape and rotation.  Eleven suites /
  59 cases total.

### Changed
- 📈 `get_server_stats` responses are unchanged, but the admin panel's data
  now has an offline twin: the audit trail can answer "who blocked this
  user" without touching the server.

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
