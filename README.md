<div align="center">

<img src="docs/images/hero.webp" alt="IUT-Food" width="220"/>

# 🍔 IUT-Food

**A campus food-ordering system built with Qt / C++17 — client, TCP server and SQLite, fully working end-to-end.**

[![CI](https://github.com/Abolfazlrwm/IUT_Food/actions/workflows/ci.yml/badge.svg)](https://github.com/Abolfazlrwm/IUT_Food/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/Abolfazlrwm/IUT_Food?include_prereleases&logo=github)](https://github.com/Abolfazlrwm/IUT_Food/releases)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![Qt](https://img.shields.io/badge/Qt-%E2%89%A55.15%20%7C%206.x-41CD52?logo=qt&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)

*فارسی? The UI is Persian — the docs below are in English. [پایین را ببینید 👇](#-پروژه)*

</div>

---

## ✨ What is this?

IUT-Food is a food-ordering platform for university students (built as a software-engineering
course project at Isfahan University of Technology). It ships as **two Qt applications**:

| App | What it does |
|---|---|
| **`iut-food-client`** (GUI) | Customers browse restaurants, order food, pay from a wallet, redeem coupons, track orders live and chat. Restaurant owners manage their profile, menu and incoming orders. Admins watch over users, coupons, orders and live reports. |
| **`iut-food-server`** (console) | A TCP server (newline-delimited JSON) that owns the remote database, authenticates logins, and **broadcasts order-status changes, chat messages and new-order events to every connected client in real time**. |

The client is offline-first: it keeps a local SQLite cache and degrades gracefully when the
server is not running, then syncs through the server whenever it is available.

## 🖼 Screenshots

| Launcher | Customer workspace |
|---|---|
| ![launcher](docs/screenshots/01_launcher.webp) | ![client](docs/screenshots/03_client.webp) |

| Menu with favorites | Checkout with coupon + wallet |
|---|---|
| ![menu](docs/screenshots/05_menu.webp) | ![checkout](docs/screenshots/06_checkout.webp) |

| 🌙 Dark mode | Live order tracking |
|---|---|
| ![dark](docs/screenshots/11_client_dark.webp) | ![tracking](docs/screenshots/08_tracking.webp) |

| Wallet page | Admin panel with coupon manager |
|---|---|
| ![wallet](docs/screenshots/07_profile_wallet.webp) | ![admin](docs/screenshots/09_admin.webp) |

## 🚀 Features

### Ordering experience
- 👤 **Customer** — search & filter restaurants, browse menus, shopping cart, checkout,
  order history, delivery confirmation, star ratings with comments, live chat per order
- 💳 **Wallet** — top up a balance, pay orders from the wallet (transactional, atomic
  deduction, full transaction log), fall back to cash
- 🎟 **Coupon codes** — percent or fixed-amount discounts with minimum-order thresholds,
  usage limits and expiry; admins manage them, customers redeem them at checkout
- 🚦 **Live order tracking** — a real-time pipeline widget (`placed → confirmed → shipped
  → delivered`) that updates in place as the restaurant works on the order
- 🔄 **One-click reorder** — push a previous order's dishes back into the cart
- ⭐ **Favorites** — bookmark restaurants and filter the list down to them

### Platform
- 🏪 **Restaurant owner** — login/registration (admin-approved), profile editor,
  full menu CRUD, incoming-order dashboard, one-click *confirm → ship* transitions
- 🛡 **Admin** — user management (block / unblock / delete), restaurant approval workflow,
  order browser, coupon manager, and **four live reports** (orders by status, revenue per
  restaurant, most active restaurants, average rating per restaurant) rendered with Qt Charts
- 🌙 **Dark & light themes** — full application-wide stylesheets, switched live from any
  window and persisted across restarts
- 🔌 **Real-time server push** — status changes, chat messages and new-order events are
  broadcast over TCP; every window of every process updates instantly
- 💾 **Offline-first client** — local SQLite cache; the app is fully usable without the server
- 🌱 **Self-seeding demo data** — first launch creates restaurants, menus, orders, a review,
  wallet balance and coupons, so every screen has content immediately
- 🎨 **Bundled Vazirmatn typeface** + full right-to-left UI — Persian renders identically on
  Windows, Linux and macOS (no more broken Tahoma)
- 🔒 **Hardened auth** — salted PBKDF2-HMAC-SHA256 password hashing with a transparent
  one-time upgrade of pre-v2.1 unsalted hashes; per-connection rate limiting (token bucket)
  and a 64 KiB message cap on the wire
- 💾 **Parameterised SQL everywhere**, canonical order-state machine, smooth schema
  migration (v1/v2 databases upgrade in place)

## 🏗 Architecture

```mermaid
flowchart LR
    subgraph client["iut-food-client (Qt Widgets)"]
        UI["GUI windows\n(customer / restaurant / admin)"]
        NM["NetworkManager\nsingleton TCP client"]
        DB[("local SQLite cache\niut_food.db")]
        UI --> NM
        UI --> DB
    end

    subgraph server["iut-food-server (QCoreApplication)"]
        SRV["Server (QTcpServer)\nbroadcast hub"]
        CH["ConnectionHandler\n1 per client"]
        SDB[("server.db")]
        SRV --> CH --> SDB
    end

    NM -- "newline-delimited JSON\nport 1234" --> SRV
    SRV -- "responses + push notifications" --> NM
```

**Order lifecycle** (canonical states, shown in Persian in the UI):

```mermaid
stateDiagram-v2
    [*] --> pending : customer checks out
    pending --> confirmed : restaurant accepts
    confirmed --> shipped : restaurant ships
    shipped --> delivered : customer confirms receipt
    delivered --> [*] : rating + review
    pending --> cancelled : rejected / insufficient wallet
```

The full JSON command reference lives in [`docs/PROTOCOL.md`](docs/PROTOCOL.md).

## 🗄 Database

Nine tables — `users` (customers, restaurants and admins in one table with a `role`
column + wallet `balance`), `foods`, `orders` (with discount/coupon/payment columns),
`order_items` (with a denormalised food-name snapshot so history survives menu edits),
`restaurants` (server-sync cache), `reviews`, plus the v2 tables `wallet_transactions`,
`coupons` and `favorites`:

```mermaid
erDiagram
    users ||--o{ foods : "owns menu"
    users ||--o{ orders : "places / receives"
    orders ||--|{ order_items : "contains"
    orders ||--o| reviews : "rated by"
    users ||--o{ wallet_transactions : "wallet log"
    users ||--o{ favorites : "bookmarks"
    coupons ||..o{ orders : "discounts"
```

## 📦 Build

### Prerequisites

- **Qt 5.15+ or Qt 6.2+** with the modules `Widgets`, `Network`, `Sql`, `Charts`
  - WebP icons require the **Qt Image Formats** module (bundled in standard Qt installs)
- **CMake ≥ 3.16** (recommended) *or* qmake (any Qt Creator version)

### Option A — CMake (recommended)

```bash
git clone https://github.com/Abolfazlrwm/IUT_Food.git
cd IUT-Food
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# run the server (terminal 1)
./build/iut-food-server
# run the client (terminal 2)
./build/iut-food-client
```

### Option B — qmake / Qt Creator

Open `IUT-Food.pro` in Qt Creator → *Configure Project* → build & run. It builds both
`src/client/client.pro` and `src/server/server.pro`.

> **Windows:** run the server from a terminal (`iut-food-server.exe`), then the client.
> Both executables must be able to reach `127.0.0.1:1234`.

### Option C — Docker (server)

```bash
docker compose up          # builds the image and serves on 1234 with a named volume
```

The image is a two-stage build (distro Qt 6 toolchain → slim runtime with only the
server and its Qt libraries) and is built *and* smoke-tested inside CI on every push.

## ▶ Quick tour (2 minutes)

1. Start `iut-food-server` (keeps running — it prints every connection).
2. Start `iut-food-client`.
3. **سفارش غذا** → log in with `demo_customer` / `123456`.
4. Show restaurants → pick **پیتزا پاردیس** → add dishes → cart → checkout.
5. At checkout type coupon **`WELCOME20`** → watch 20 % come off, then pay from the wallet 🪙
6. A live **tracking window** opens — start a **second** client instance →
   **پنل رستوران** → log in with `pizza_pardis` / `123456` → confirm & ship the order →
   watch the tracker jump steps in the first window instantly. Say hi in the chat tab 🎉
7. Profile → **کیف پول** → top up, check the transaction log; open an old order → **🔄 سفارش دوباره**.
8. **پنل مدیریت** (any client) → manage users, create coupons, render the live charts.

### Demo accounts (seeded automatically on first run)

| Role | Username | Password | Extra |
|---|---|---|---|
| Admin | `admin` | `admin123` | coupon manager |
| Customer | `demo_customer` | `123456` | wallet pre-charged with 200,000 تومان |
| Restaurants | `kebab_hafez`, `pizza_pardis`, `burger_bam` | `123456` | |

Demo coupons: `WELCOME20` (20 % off, min 100,000) and `IUT50` (50,000 تومان off, min 250,000).

### Server options

```
iut-food-server [--port 1234] [--db server.db]
```

## 🧪 Tests

### Unit tests — six Qt Test suites, 33 cases

The business rules are covered by Qt Test suites that run against a scratch SQLite
database (and one suite against the official RFC vectors):

| Suite | Covers |
|---|---|
| `passwordhasher_tests` | PBKDF2-HMAC-SHA256 against the **official RFC vectors**, salt uniqueness, tampering rejection |
| `ratelimiter_tests` | token-bucket burst, refill, cap and flood shedding |
| `wallet_tests` | atomic payment, overdraw rejection, transaction audit log |
| `coupon_tests` | percent/amount math, min-order, expiry, usage limits |
| `orders_tests` | canonical status machine, item snapshots, cancel path |
| `users_tests` | registration/login, blocking, **transparent legacy-hash migration** |

```bash
ctest --test-dir build --output-on-failure    # 6 suites, 33 cases
```

### End-to-end smoke test — 35 protocol checks

`scripts/smoke_test.py` speaks the wire protocol against a running server and exercises
every command — login, filters, menu, ordering, status pushes, chat broadcast, reviews,
wallet, coupons and user administration.

```bash
./build/iut-food-server &        # start the server
python3 scripts/smoke_test.py    # ✅ 35 checks
```

### Performance

`scripts/benchmark.py` drives 50 concurrent client connections and records every
round trip (scratch database, read-mostly workload):

| Requests | Throughput | p50 | p90 | p95 | p99 |
|---|---|---|---|---|---|
| 1000 | 394 req/s | 1.6 ms | 1.9 ms | 2.2 ms | 3.2 ms |

![benchmark](docs/images/benchmark.webp)

Logins intentionally cost one PBKDF2 derivation on the server (~30 ms each); the
`max` tail in the chart is the initial 50-client login stampede draining the
single-threaded event loop.

The GitHub Actions workflow builds both targets on every push, runs `ctest` and the
smoke test, and builds + smoke-tests the Docker image in its own job.

## 📁 Project structure

```
IUT-Food/
├── src/
│   ├── common/          # shared data layer (SQLite, models, status machine, demo seed)
│   ├── client/          # Qt Widgets GUI (customer, restaurant, admin) + TCP client
│   └── server/          # console TCP server (broadcasts + JSON command dispatch)
├── resources/           # qrc: WebP icons + bundled Vazirmatn fonts
├── fonts/               # Vazirmatn (SIL OFL 1.1)
├── docs/
│   ├── PROTOCOL.md      # full JSON protocol reference
│   ├── images/          # hero banner (WebP)
│   └── screenshots/     # real app screenshots (WebP)
├── scripts/
│   ├── smoke_test.py         # end-to-end protocol test (35 checks)
│   ├── benchmark.py          # latency / throughput harness
│   └── screenshot_driver.cpp # offscreen capture driver for the docs
├── tests/               # six Qt Test suites (run with ctest)
├── Dockerfile           # two-stage server image (+ docker-compose.yml)
├── .github/workflows/   # CI: build, unit tests, smoke test, docker image
└── CMakeLists.txt       # two targets: iut-food-client, iut-food-server
```

## 🛣 Roadmap

- [x] Wallet & coupon system (v2.0)
- [x] Live order tracking pipeline (v2.0)
- [x] Dark/light themes (v2.0)
- [x] Salted password hashing (PBKDF2-HMAC-SHA256) with transparent legacy migration (v2.1)
- [x] Unit tests, Docker image and latency benchmark (v2.1)
- [ ] Real authentication over the wire (session tokens) — admin commands are currently
      trusted by the server
- [ ] TLS transport (QSslServer) for remote deployments
- [ ] Per-order chat rooms instead of global broadcast
- [ ] WebSocket transport option
- [ ] Telegram bot for order notifications

## 🤝 Contributing

Issues and PRs are welcome — this is a student project that is meant to be poked at.
See [CONTRIBUTING.md](CONTRIBUTING.md); please keep the build green (CI runs the smoke
test) and match the existing code style (4 spaces, `QStringLiteral`, documented classes).

## 📜 License

- Project code: **MIT** — see [LICENSE](LICENSE).
- [Vazirmatn font](https://github.com/rastikerdar/vazirmatn): SIL Open Font License 1.1.

---

<div align="center">

## 🍔 پروژه

**IUT-Food** یک سامانه سفارش غذای دانشگاهی است که با Qt و C++17 ساخته شده:
کلاینت گرافیکی برای مشتری، رستوران و مدیر + سرور TCP با قابلیت push لحظه‌ای وضعیت سفارش‌ها و چت.
در نسخه ۲، **کیف پول، کدهای تخفیف، رهگیری زنده سفارش، حالت تاریک و علاقه‌مندی‌ها** هم اضافه شده است.
رابط کاربری کاملاً فارسی و راست‌به‌چپ است و داده‌های نمونه در اولین اجرا ساخته می‌شوند.

⭐ اگر این پروژه برایتان مفید بود، یک ستاره بزنید!

</div>
