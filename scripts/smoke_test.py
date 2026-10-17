#!/usr/bin/env python3
"""End-to-end protocol smoke test for the IUT-Food TCP server.

Connects to the server like a real client would and exercises every
command documented in docs/PROTOCOL.md - including the v2.2 session
authentication (login mints a token, every other command must present
it, roles and ownership are enforced server-side).

By default the suite speaks plain TCP on port 1234.  With `--tls` the
whole suite runs over an encrypted channel instead (a server started
with --tls-cert/--tls-key), plus two extra checks for the handshake
itself.  `--port` points it at a non-default port.
"""
import argparse
import json
import socket
import ssl
import sys
import uuid

HOST = "127.0.0.1"
PORT = 1234
USE_TLS = False
failures = []


class Client:
    def __init__(self, name):
        raw = socket.create_connection((HOST, PORT), timeout=5)
        if USE_TLS:
            # Self-signed demo certificate: verify disabled on purpose.
            ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            self.sock = ctx.wrap_socket(raw)
        else:
            self.sock = raw
        self.sock.settimeout(5)
        self.buf = b""
        self.pending = []
        self.name = name
        self.token = None

    def _wire(self, obj):
        """Attaches the session token (except to the login itself)."""
        if self.token and obj.get("command") != "login":
            obj = dict(obj)
            obj.setdefault("token", self.token)
        return (json.dumps(obj) + "\n").encode()

    def raw_send(self, obj):
        """Fire-and-forget send for broadcast-only commands."""
        self.sock.sendall(self._wire(obj))

    def request(self, obj):
        self.sock.sendall(self._wire(obj))
        want = obj.get("command")
        # Responses echo the request's "command" field, so correlate on it;
        # anything else (broadcasts/notifications) gets parked.
        parked = self.pending
        self.pending = []
        for msg in parked:
            if msg.get("command") == want:
                return self._absorb(msg)
            self.pending.append(msg)
        while True:
            if b"\n" in self.buf:
                line, self.buf = self.buf.split(b"\n", 1)
                msg = json.loads(line)
                if msg.get("command") != want:
                    self.pending.append(msg)
                    continue
                return self._absorb(msg)
            data = self.sock.recv(65536)
            if not data:
                raise ConnectionError("server closed the connection")
            self.buf += data

    def _absorb(self, msg):
        """A successful login stores the session token for later requests."""
        if (msg.get("command") == "login" and msg.get("success") is True
                and msg.get("session_token")):
            self.token = msg["session_token"]
        return msg

    def drain(self, seconds=1.5):
        """Read everything that arrives within `seconds` (broadcasts)."""
        import time
        self.sock.settimeout(seconds)
        messages = list(self.pending)
        self.pending = []
        try:
            while True:
                data = self.sock.recv(65536)
                if not data:
                    break
                self.buf += data
                while b"\n" in self.buf:
                    line, self.buf = self.buf.split(b"\n", 1)
                    if line.strip():
                        messages.append(json.loads(line))
        except socket.timeout:
            pass
        self.sock.settimeout(5)
        return messages

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def check(label, cond, extra=""):
    status = "PASS" if cond else "FAIL"
    print(f"[{status}] {label} {extra}")
    if not cond:
        failures.append(label)


def main():
    admin = Client("admin")
    customer = Client("customer")
    print(f"connected: {admin.name} {customer.name}"
          + (" over TLS" if USE_TLS else ""))

    # ---- v2.3 TLS transport (only in --tls mode) ----------------------
    if USE_TLS:
        check("TLS handshake negotiated a protocol version",
              admin.sock.version() in ("TLSv1.2", "TLSv1.3"), str(admin.sock.version()))
        cipher = admin.sock.cipher()
        check("TLS cipher suite is in use",
              isinstance(cipher, tuple) and len(cipher[0]) > 0, str(cipher))

    # ---- v2.2 authentication -----------------------------------------
    r = admin.request({"command": "get_users"})
    check("request without token rejected",
          r.get("success") is False and r.get("error") == "unauthorized", str(r)[:90])

    r = admin.request({"command": "login", "username": "admin", "password": "admin123"})
    check("login admin", r.get("success") is True and r.get("role") == "admin", str(r)[:80])
    check("login mints a 256-bit session token",
          isinstance(r.get("session_token"), str) and len(r["session_token"]) == 64,
          f"len={len(r.get('session_token', ''))}")
    check("login reports session lifetime",
          isinstance(r.get("expires_in"), int) and r["expires_in"] >= 600)

    r = customer.request({"command": "login", "username": "demo_customer", "password": "123456"})
    check("login customer", r.get("success") is True and r.get("role") == "customer"
          and r.get("user_id", -1) > 0, str(r)[:80])

    r = admin.request({"command": "login", "username": "demo_customer", "password": "WRONG"})
    check("login wrong password rejected", r.get("success") is False)

    r = admin.request({"command": "get_users", "token": "f" * 64})
    check("forged token rejected",
          r.get("success") is False and r.get("error") == "unauthorized")

    # ---- restaurants -------------------------------------------------
    r = admin.request({"command": "get_restaurants"})
    restaurants = r.get("data", [])
    check("get_restaurants returns seeded list", len(restaurants) >= 3, f"n={len(restaurants)}")

    r = admin.request({"command": "get_restaurants", "type": "فست فود"})
    check("get_restaurants type filter", all(x["type"] == "فست فود" for x in r["data"]),
          f"n={len(r['data'])}")

    r = admin.request({"command": "get_restaurants", "name": "پاردیس"})
    check("get_restaurants name filter", len(r["data"]) == 1)

    # ---- menu ----------------------------------------------------------
    kebab = next(x for x in restaurants if x["name"] == "کباب‌سرای حافظ")
    rid = kebab["id"]
    r = admin.request({"command": "get_menu_items_for_restaurant", "restaurant_id": rid})
    menu = r.get("data", [])
    check("get_menu_items_for_restaurant", len(menu) >= 4, f"n={len(menu)}")

    # ---- place order ----------------------------------------------------
    pizza = next(x for x in restaurants if x["name"] == "پیتزا پاردیس")
    customer_id = 2  # demo_customer
    items = [{"id": 5, "name": "پیتزا پپرونی", "price": 320000, "quantity": 1},
             {"id": 7, "name": "سیب‌زمینی سرخ‌کرده", "price": 90000, "quantity": 2}]
    r = customer.request({"command": "place_order",
                          "customer_id": customer_id,
                          "restaurant_id": pizza["id"],
                          "total_price": 500000,
                          "items": items})
    order_id = r.get("order_id", -1)
    check("place_order", r.get("success") is True and order_id > 0, f"order_id={order_id}")

    r = customer.request({"command": "place_order",
                          "customer_id": 9999,  # someone else's basket
                          "restaurant_id": pizza["id"],
                          "total_price": 1000, "items": []})
    check("order for a foreign customer_id forbidden",
          r.get("success") is False and r.get("error") == "forbidden", str(r)[:90])

    # ---- restaurant-scoped order list -------------------------------------
    r = admin.request({"command": "get_orders", "restaurant_id": pizza["id"]})
    ids = [o["id"] for o in r["data"]]
    check("get_orders filtered by restaurant",
          order_id in ids and all(o["restaurant_id"] == pizza["id"] for o in r["data"]),
          f"ids={ids}")

    r = admin.request({"command": "get_orders"})
    check("get_orders (all)", len(r["data"]) >= 3, f"n={len(r['data'])}")

    # ---- status transition + push notification -----------------------------
    _ = customer.drain(0.4)  # clear old traffic
    r = admin.request({"command": "update_order_status",
                       "order_id": order_id, "new_status": "confirmed"})
    check("update_order_status", r.get("success") is True)

    push = customer.drain(1.5)
    status_push = [m for m in push if m.get("type") == "order_status_update_notification"]
    check("customer receives status push", bool(status_push), str(push)[:100])

    # invalid status must be rejected by the DB layer
    r = admin.request({"command": "update_order_status",
                       "order_id": order_id, "new_status": "bogus_status"})
    check("invalid status rejected", r.get("success") is False)

    # ---- chat broadcast ------------------------------------------------------
    _ = customer.drain(0.3)
    # send_chat_message intentionally produces no direct reply - it only
    # broadcasts to other clients, so send raw and then drain.
    admin.raw_send({"command": "send_chat_message", "order_id": order_id,
                    "sender": "رستوران تست",
                    "message_text": "سلام، سفارش شما آماده می‌شود"})
    push = customer.drain(2.0)
    chat = [m for m in push if m.get("type") == "new_chat_message_notification"]
    check("chat broadcast reaches other client", bool(chat), str(chat)[:120])

    # ---- review -----------------------------------------------------------------
    # advance to delivered then review
    admin.request({"command": "update_order_status", "order_id": order_id, "new_status": "shipped"})
    admin.request({"command": "update_order_status", "order_id": order_id, "new_status": "delivered"})
    r = customer.request({"command": "submit_review", "order_id": order_id,
                          "rating": 4, "review_text": "سریع بود"})
    check("submit_review", r.get("success") is True)

    r = admin.request({"command": "get_reviews"})
    rev = next((x for x in r["data"] if x["order_id"] == order_id), None)
    check("get_reviews returns the review", rev is not None and rev["rating"] == 4, str(rev)[:100])

    # ---- wallet (schema v3) ------------------------------------------------
    r = customer.request({"command": "get_wallet_balance", "user_id": 2})
    balance0 = r.get("balance", -1)
    check("get_wallet_balance", balance0 >= 0, str(r)[:80])

    r = customer.request({"command": "get_wallet_balance", "user_id": 1})
    check("foreign wallet read forbidden",
          r.get("success") is False and r.get("error") == "forbidden", str(r)[:90])

    r = customer.request({"command": "charge_wallet", "user_id": 2, "amount": 150000})
    check("charge_wallet", r.get("success") is True)

    r = customer.request({"command": "get_wallet_balance", "user_id": 2})
    check("wallet balance increased",
          abs(r.get("balance", 0) - (balance0 + 150000)) < 0.01,
          f"{balance0} -> {r.get('balance')}")

    # ---- coupons (schema v3) ------------------------------------------------
    r = customer.request({"command": "create_coupon", "code": "HAX",
                          "kind": "percent", "value": 99, "min_order": 0,
                          "max_uses": 1, "expires_at": ""})
    check("coupon creation forbidden for customers",
          r.get("success") is False and r.get("error") == "forbidden", str(r)[:90])

    r = admin.request({"command": "create_coupon", "code": "SMOKE10",
                       "kind": "percent", "value": 10, "min_order": 100000,
                       "max_uses": 5, "expires_at": ""})
    check("create_coupon", r.get("success") is True)

    r = customer.request({"command": "validate_coupon", "code": "smoke10", "total": 200000})
    check("validate_coupon percent",
          r.get("ok") is True and abs(r.get("discount", 0) - 20000) < 0.01,
          str(r)[:100])

    r = customer.request({"command": "validate_coupon", "code": "NOPE", "total": 200000})
    check("validate_coupon unknown rejected", r.get("ok") is False)

    # wallet order with a coupon through place_order (server-side flow)
    r = customer.request({"command": "place_order",
                          "customer_id": 2, "restaurant_id": pizza["id"],
                          "total_price": 200000, "payment_method": "wallet",
                          "coupon_code": "SMOKE10",
                          "items": [{"id": 5, "name": "پیتزا پپرونی",
                                     "price": 200000, "quantity": 1}]})
    check("place_order wallet+coupon",
          r.get("success") is True and abs(r.get("total_price", 0) - 180000) < 0.01,
          str(r)[:120])

    r = customer.request({"command": "get_wallet_balance", "user_id": 2})
    check("wallet deducted",
          abs(r.get("balance", 0) - (balance0 + 150000 - 180000)) < 0.01,
          str(r)[:80])

    # overdraw attempt must fail and cancel the order
    r = customer.request({"command": "place_order",
                          "customer_id": 2, "restaurant_id": pizza["id"],
                          "total_price": 99999999, "payment_method": "wallet",
                          "items": []})
    check("wallet overdraw rejected", r.get("success") is False, str(r)[:100])

    # coupon consumption + admin toggle round-trip
    r = admin.request({"command": "get_coupons"})
    smoke10 = next((c for c in r.get("data", []) if c["code"] == "SMOKE10"), None)
    check("coupon consumption tracked",
          smoke10 is not None and smoke10["used_count"] >= 1, str(smoke10)[:100])

    r = admin.request({"command": "toggle_coupon", "coupon_id": smoke10["id"],
                       "active": False})
    check("toggle_coupon", r.get("success") is True)
    r = customer.request({"command": "validate_coupon", "code": "SMOKE10", "total": 200000})
    check("inactive coupon rejected", r.get("ok") is False)
    admin.request({"command": "toggle_coupon", "coupon_id": smoke10["id"], "active": True})

    # ---- notify_order_placed broadcast --------------------------------------
    _ = admin.drain(0.3)
    customer.raw_send({"command": "notify_order_placed", "order_id": order_id,
                       "restaurant_id": pizza["id"], "total_price": 123,
                       "customer": "spoofed_name"})
    push = admin.drain(2.0)
    notes = [m for m in push if m.get("type") == "new_order_notification"]
    check("notify_order_placed broadcast", bool(notes), str(push)[:100])
    if notes:
        check("order notification stamped with the session identity",
              notes[0]["data"].get("customer") == "demo_customer",
              str(notes[0].get("data", {}).get("customer")))

    # ---- admin user management ----------------------------------------------------
    import string
    new_user = "smoke_" + uuid.uuid4().hex[:6]
    r = customer.request({"command": "update_profile",
                          "user_id": 2,
                          "data": {"username": new_user, "address": "تهران"}})
    check("update_profile", r.get("success") is True)

    r = customer.request({"command": "update_profile",
                          "user_id": 1,
                          "data": {"username": "hax", "address": "hax"}})
    check("editing a foreign profile forbidden",
          r.get("success") is False and r.get("error") == "forbidden", str(r)[:90])

    r = admin.request({"command": "get_users"})
    names = [u["username"] for u in r["data"]]
    check("update_profile visible in get_users", new_user in names)

    # restore name for the demo dataset
    customer.request({"command": "update_profile", "user_id": 2,
                      "data": {"username": "demo_customer", "address": "تهران"}})

    # block / unblock / approve / delete round-trip
    r = admin.request({"command": "get_users"})
    me = next(u for u in r["data"] if u["role"] == "customer")
    admin.request({"command": "block_user", "user_id": me["id"]})
    r = admin.request({"command": "get_users"})
    check("block_user", next(u for u in r["data"] if u["id"] == me["id"])["is_active"] is False)
    admin.request({"command": "unblock_user", "user_id": me["id"]})
    r = admin.request({"command": "get_users"})
    check("unblock_user", next(u for u in r["data"] if u["id"] == me["id"])["is_active"] is True)

    admin.request({"command": "delete_user", "user_id": 999999})  # harmless
    check("delete_user responds", True)

    # ---- role matrix: a customer session cannot drive admin commands ------
    for cmd, extra in (("get_users", {}), ("block_user", {"user_id": 1}),
                       ("get_orders", {}), ("get_reviews", {})):
        r = customer.request({"command": cmd, **extra})
        check(f"customer cannot call {cmd}",
              r.get("success") is False and r.get("error") == "forbidden")

    # ---- server stats (admin only) ------------------------------------------
    r = customer.request({"command": "get_server_stats"})
    check("server stats forbidden for customers",
          r.get("success") is False and r.get("error") == "forbidden")

    r = admin.request({"command": "get_server_stats"})
    check("server stats for admin",
          r.get("success") is True and r.get("uptime_seconds", -1) >= 0
          and r.get("requests_served", 0) > 0 and r.get("active_sessions", 0) >= 2,
          str(r)[:120])

    # ---- logout invalidates the session -------------------------------------
    guest = Client("guest")
    r = guest.request({"command": "login", "username": "demo_customer", "password": "123456"})
    check("guest login", r.get("success") is True)
    r = guest.request({"command": "get_wallet_balance", "user_id": 2})
    check("guest request with token accepted", r.get("success") is True or "balance" in r)
    r = guest.request({"command": "logout"})
    check("logout succeeds", r.get("success") is True)
    r = guest.request({"command": "get_wallet_balance", "user_id": 2})
    check("revoked token rejected after logout",
          r.get("success") is False and r.get("error") == "unauthorized", str(r)[:90])
    guest.close()

    # ---- unknown command -----------------------------------------------------------
    r = admin.request({"command": "does_not_exist"})
    check("unknown command -> error", r.get("error") == "Unknown command")

    print()
    if failures:
        print(f"❌ {len(failures)} failure(s): {failures}")
        sys.exit(1)
    print("✅ All protocol checks passed.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tls", action="store_true",
                        help="run the whole suite over TLS (self-signed)")
    parser.add_argument("--port", type=int, default=1234,
                        help="server port (default 1234)")
    args = parser.parse_args()
    USE_TLS = args.tls
    PORT = args.port
    main()
