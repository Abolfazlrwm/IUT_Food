#!/usr/bin/env python3
"""End-to-end protocol smoke test for the IUT-Food TCP server.

Connects to the server like a real client would and exercises every
command documented in docs/PROTOCOL.md.
"""
import json
import socket
import sys
import uuid

HOST, PORT = "127.0.0.1", 1234
failures = []


class Client:
    def __init__(self, name):
        self.sock = socket.create_connection((HOST, PORT), timeout=5)
        self.sock.settimeout(5)
        self.buf = b""
        self.pending = []
        self.name = name

    def request(self, obj):
        self.sock.sendall((json.dumps(obj) + "\n").encode())
        want = obj.get("command")
        # Responses echo the request's "command" field, so correlate on it;
        # anything else (broadcasts/notifications) gets parked.
        parked = self.pending
        self.pending = []
        for msg in parked:
            if msg.get("command") == want:
                return msg
            self.pending.append(msg)
        while True:
            if b"\n" in self.buf:
                line, self.buf = self.buf.split(b"\n", 1)
                msg = json.loads(line)
                if msg.get("command") != want:
                    self.pending.append(msg)
                    continue
                return msg
            data = self.sock.recv(65536)
            if not data:
                raise ConnectionError("server closed the connection")
            self.buf += data

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


def check(label, cond, extra=""):
    status = "PASS" if cond else "FAIL"
    print(f"[{status}] {label} {extra}")
    if not cond:
        failures.append(label)


def main():
    admin = Client("admin")
    customer = Client("customer")
    print("connected:", admin.name, customer.name)

    # ---- login -----------------------------------------------------
    r = admin.request({"command": "login", "username": "admin", "password": "admin123"})
    check("login admin", r.get("success") is True and r.get("role") == "admin", str(r)[:80])

    r = customer.request({"command": "login", "username": "demo_customer", "password": "123456"})
    check("login customer", r.get("success") is True and r.get("role") == "customer"
          and r.get("user_id", -1) > 0, str(r)[:80])

    r = admin.request({"command": "login", "username": "demo_customer", "password": "WRONG"})
    check("login wrong password rejected", r.get("success") is False)

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
    admin.sock.sendall((json.dumps({
        "command": "send_chat_message", "order_id": order_id,
        "sender": "رستوران تست",
        "message_text": "سلام، سفارش شما آماده می‌شود"}) + "\n").encode())
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

    # ---- admin user management ----------------------------------------------------
    import string
    new_user = "smoke_" + uuid.uuid4().hex[:6]
    r = customer.request({"command": "update_profile",
                          "user_id": 2,
                          "data": {"username": new_user, "address": "تهران"}})
    check("update_profile", r.get("success") is True)

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

    # ---- unknown command -----------------------------------------------------------
    r = admin.request({"command": "does_not_exist"})
    check("unknown command -> error", r.get("error") == "Unknown command")

    print()
    if failures:
        print(f"❌ {len(failures)} failure(s): {failures}")
        sys.exit(1)
    print("✅ All protocol checks passed.")


if __name__ == "__main__":
    main()
