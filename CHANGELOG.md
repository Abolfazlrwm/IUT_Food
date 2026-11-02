# Changelog

All notable changes to IUT-Food are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/) (admittedly loosely).

## [2.0.0] — 2026-11-03

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
