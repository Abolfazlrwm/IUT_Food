#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# IUT-Food instant demo - one command, the whole product.
#
#   scripts/run_demo.sh              plain TCP demo on 127.0.0.1:1234
#   scripts/run_demo.sh --tls        same session wrapped in self-signed TLS
#   scripts/run_demo.sh --metrics    also expose Prometheus on :9101
#   scripts/run_demo.sh --no-client  server only (headless machines/CI)
#
# What it does:
#   1. finds (or builds) the client and server binaries,
#   2. backs up the database next to the binaries, then lets the server
#      seed a FRESH demo database (restored when the demo exits),
#   3. starts the server and waits for it with the built-in --healthcheck
#      probe (the same one Docker uses),
#   4. launches the client so the product is on screen within a minute.
#
# Logins accepted by the seeded demo database (also printed inside the
# client's login dialog):
#   customer    demo_customer / 123456
#   restaurant  kebab_hafez   / 123456
#   admin       admin         / admin123
# ---------------------------------------------------------------------------
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT=1234
METRICS=0
WITH_TLS=0
NO_CLIENT=0

for arg in "$@"; do
    case "$arg" in
        --tls)      WITH_TLS=1 ;;
        --metrics)  METRICS=1 ;;
        --no-client) NO_CLIENT=1 ;;
        -h|--help)  sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $arg (see --help)"; exit 2 ;;
    esac
done

# ---- locate (or produce) the binaries -----------------------------------
find_client() {
    for candidate in \
        "${IUT_FOOD_BUILD_DIR:-}/iut-food-client" \
        "$REPO_ROOT/build/linux-release/iut-food-client" \
        "$REPO_ROOT/build/iut-food-client"; do
        [ -x "$candidate" ] && { printf '%s\n' "$candidate"; return 0; }
    done
    return 1
}

CLIENT_BIN="$(find_client)" || {
    echo "No build output found - building with the linux-release preset..."
    cmake --preset linux-release -S "$REPO_ROOT"
    cmake --build --preset linux-release
    CLIENT_BIN="$REPO_ROOT/build/linux-release/iut-food-client"
}
SERVER_BIN="${CLIENT_BIN%/*}/iut-food-server"
[ -x "$SERVER_BIN" ] || { echo "server binary missing next to $CLIENT_BIN"; exit 1; }

echo "client: $CLIENT_BIN"
echo "server: $SERVER_BIN"

# ---- fresh demo database (backup yours; restored on exit) ---------------
DB_PATH="${CLIENT_BIN%/*}/iut_food.db"
if [ -f "$DB_PATH" ]; then
    BACKUP_DB="$(mktemp -u /tmp/iut-food-demo-db.XXXXXX)"
    mv "$DB_PATH" "$BACKUP_DB"
    echo "your existing database is parked at $BACKUP_DB (restored on exit)"
fi
restore_db() {
    [ -n "${BACKUP_DB:-}" ] && [ -f "${BACKUP_DB:-}" ] \
        && mv -f "$BACKUP_DB" "$DB_PATH" || rm -f "$DB_PATH"
    [ -n "${SERVER_PID:-}" ] && kill "$SERVER_PID" 2>/dev/null || true
}
trap restore_db EXIT

# ---- optional TLS certificates ------------------------------------------
if [ "$WITH_TLS" -eq 1 ]; then
    sh "$REPO_ROOT/scripts/generate_certs.sh" >/dev/null
    TLS_FLAGS=(--tls-cert "$REPO_ROOT/certs/server.crt" --tls-key "$REPO_ROOT/certs/server.key")
    # Pre-seed the client's QSettings so the login dialog shows up with
    # TLS already ticked - zero clicks for the demo.
    CONF_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/IUT"
    mkdir -p "$CONF_DIR"
    CONF="$CONF_DIR/IUT-Food.conf"
    touch "$CONF"
    for kv in "connection\\\\host=127.0.0.1" "connection\\\\port=$PORT" \
              "connection\\\\tls=true" "connection\\\\tlsTrust=true"; do
        key="${kv%%=*}"; value="${kv#*=}"
        if grep -q "^$key=" "$CONF" 2>/dev/null; then
            sed -i "s|^$key=.*|$key=$value|" "$CONF"
        else
            printf '%s=%s\n' "$key" "$value" >> "$CONF"
        fi
    done
    echo "TLS: self-signed demo certificate generated, client pre-configured"
else
    TLS_FLAGS=()
fi

METRICS_FLAGS=()
[ "$METRICS" -eq 1 ] && METRICS_FLAGS=(--metrics-port 9101)

# ---- start the server, wait for the ping --------------------------------
echo "starting server on 127.0.0.1:$PORT (log: /tmp/iut-food-demo-server.log)"
"$SERVER_BIN" --port "$PORT" "${TLS_FLAGS[@]}" "${METRICS_FLAGS[@]}" \
    > /tmp/iut-food-demo-server.log 2>&1 &
SERVER_PID=$!

for _ in $(seq 1 40); do
    if "$SERVER_BIN" --healthcheck --port "$PORT" >/dev/null 2>&1; then
        HEALTHY=1; break
    fi
    sleep 0.5
done
if [ "${HEALTHY:-0}" -ne 1 ]; then
    echo "server did not become healthy within 20s - log follows:"
    tail -20 /tmp/iut-food-demo-server.log
    exit 1
fi
echo "server healthy: $(tail -1 /tmp/iut-food-demo-server.log | head -c 120)"
[ "$METRICS" -eq 1 ] && echo "Prometheus metrics: http://127.0.0.1:9101/metrics"

# ---- hand over to the GUI ----------------------------------------------
if [ "$NO_CLIENT" -eq 1 ]; then
    echo "headless mode: press Ctrl+C to stop the server"
    wait "$SERVER_PID"
else
    echo
    echo "=========================================================="
    echo " demo logins:  demo_customer/123456   kebab_hafez/123456"
    echo "               admin/admin123"
    echo " closing the client window stops the demo"
    echo "=========================================================="
    echo
    "$CLIENT_BIN"
fi
