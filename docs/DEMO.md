# IUT-Food in 60 seconds — the demo tour

Everything ships ready: the server seeds a demo database on first run,
the client knows how to reach it, and one script wires both together.

```bash
git clone https://github.com/Abolfazlrwm/IUT_Food.git
cd IUT_Food
scripts/run_demo.sh            # builds if needed, then runs the product
```

That is the whole setup. The script parks your real database aside,
lets the server seed a fresh one, waits for the built-in `--healthcheck`
probe to say *healthy*, and puts the launcher on screen. Closing the
client window stops the demo and restores your database.

## The tour (what to click, in order)

1. **Launcher** — three doors: customer, restaurant dashboard, admin.
   Hover them: each explains itself. `F1` opens About, `Ctrl+Q` quits.
2. **Customer** — sign in as `demo_customer / 123456`. The status bar
   now pins two facts permanently: the link state (`متصل به سرور 🟢`,
   with a `🔒TLS` badge on encrypted sessions) and the server release
   (`سرور v2.8.0`) that the login handshake announced.
3. **Browse & order** — type in the search box (`Ctrl+F` jumps into it),
   filter by cuisine or campus zone, open a menu, add food to the cart,
   check out and pay **from the wallet**. The live tracker opens by
   itself and follows the order pipeline.
4. **Restaurant side** — in a second terminal run
   `scripts/run_demo.sh --no-client`, then log in as the restaurant
   `kebab_hafez / 123456`. Confirm the order you just placed and watch
   the customer's tracker move in real time — the push notification
   crosses the wire between two independent processes.
5. **Admin** — sign in as `admin / admin123`: approve the pending
   restaurant, browse the live reports, and try the **CSV export**
   buttons (RFC 4180 + UTF-8 BOM, so Persian headers survive Excel).
6. **Dark mode** — the theme switch lives in the toolbar/menu bar of
   every window and persists across restarts.

## Variants

```bash
scripts/run_demo.sh --tls        # generate_certs.sh + encrypted session
scripts/run_demo.sh --metrics    # also scrape http://127.0.0.1:9101/metrics
scripts/run_demo.sh --no-client  # server only, for the two-terminal tour
```

With `--tls` the login dialog opens with TLS already ticked and the
status bar badge turns `🔒TLS` — the wire is now encrypted (self-signed
demo certificate from `scripts/generate_certs.sh`, development only).

With `--metrics` the server answers Prometheus scrapes on port 9101:

```console
$ curl -s 127.0.0.1:9101/metrics | head -4
# HELP iutfood_build_info Build metadata
# TYPE iutfood_build_info gauge
iutfood_build_info{version="2.8.0",qt="6.8.3"} 1
```

Scrape config and the full metric reference: [METRICS.md](METRICS.md).

## Without the script (manual)

```bash
cmake --preset linux-release && cmake --build --preset linux-release
./build/linux-release/iut-food-server --port 1234 &   # seeds demo data
./build/linux-release/iut-food-client                 # picks up the same DB
```

## Notes

- The demo database lives next to the binaries (`build/linux-release/`);
  the launcher, the client, the restaurant dashboard and the admin panel
  all share it, so the whole story stays consistent.
- A *fresh* database always contains the seeded restaurants, menu items,
  wallet balances, coupons and the three demo accounts above
  (`src/common/demodata.*`).
- Windows: use `cmake --preset windows-release`, then start the two
  binaries by hand (step *Without the script*).
- Every wire message is documented in [PROTOCOL.md](PROTOCOL.md); the
  screenshot gallery in this repository is captured from these very
  windows by `scripts/screenshot_driver.cpp`.
