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

---

## 1. Authentication

### `login`
```json
→ {"command": "login", "username": "demo_customer", "password": "123456"}

← {"type": "response", "command": "login", "success": true,
   "role": "customer", "user_id": 2}
```
Passwords are stored hashed (SHA-256) on the server; the raw password is
only ever visible on the wire of this demo protocol (see Roadmap).

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
```json
→ {"command": "place_order", "customer_id": 2, "restaurant_id": 4,
   "total_price": 410000,
   "items": [{"id": 5, "name": "پیتزا پپرونی", "price": 320000, "quantity": 1}]}

← {"type": "response", "command": "place_order", "success": true, "order_id": 3}
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

## 7. Errors

Unknown commands answer immediately:
```json
→ {"command": "does_not_exist"}
← {"type": "response", "command": "does_not_exist", "error": "Unknown command"}
```
Malformed lines (not JSON, no `"command"`) are dropped with a log entry.
