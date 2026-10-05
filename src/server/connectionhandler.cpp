#include "connectionhandler.h"
#include "databasehandler.h"
#include "logger.h"
#include "orderstatus.h"
#include "server.h"

#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlRecord>

ConnectionHandler::ConnectionHandler(QTcpSocket *socket, DataBaseHandler *dbHandler,
                                     SessionManager *sessions, ServerStats *stats,
                                     QObject *parent)
    : QObject(parent), m_dbHandler(dbHandler), m_sessions(sessions), m_stats(stats)
{
    m_socket = socket;
    m_socket->setParent(this);

    connect(m_socket, &QTcpSocket::readyRead, this, &ConnectionHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ConnectionHandler::onDisconnected);

    LOG_DEBUG("conn", QString("new connection from %1")
                          .arg(m_socket->peerAddress().toString()));
}

ConnectionHandler::~ConnectionHandler()
{
    LOG_DEBUG("conn", "handler destroyed");
}

void ConnectionHandler::sendJson(const QJsonObject &obj)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    m_socket->write(payload);
    m_socket->flush();
}

void ConnectionHandler::closeConnection()
{
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState)
        m_socket->disconnectFromHost(); // flushes the goodbye first
}

void ConnectionHandler::onReadyRead()
{
    // Guard 1: a single message that never terminates is dropped before it
    // can grow the buffer without bound.
    if (m_socket->bytesAvailable() > MaxMessageBytes && !m_socket->canReadLine()) {
        rejectAbuse("Message too large");
        return;
    }

    while (m_socket->canReadLine()) {
        const QByteArray line = m_socket->readLine().trimmed();
        if (line.isEmpty())
            continue;

        // Guard 2: oversize line (even after a terminating newline arrived).
        if (line.size() > MaxMessageBytes) {
            rejectAbuse("Message too large");
            return;
        }

        // Guard 3: per-connection request budget (token bucket).  Flooding
        // connections get one explanatory reply and are disconnected.
        if (!m_limiter.allow(QDateTime::currentMSecsSinceEpoch())) {
            rejectAbuse("Too many requests");
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) {
            qDebug() << "Invalid JSON from client.";
            continue;
        }
        processCommand(doc.object());
    }
}

void ConnectionHandler::rejectAbuse(const char *message)
{
    LOG_WARNING("guard", QString("rejecting client %1: %2")
                             .arg(m_socket->peerAddress().toString(),
                                  QString::fromLatin1(message)));
    if (m_stats)
        m_stats->requestsRejected.fetch_add(1, std::memory_order_relaxed);
    QJsonObject err;
    err[QLatin1String("type")] = QLatin1String("rate_limit_error");
    err[QLatin1String("message")] = QString::fromLatin1(message);
    sendJson(err);
    m_socket->disconnectFromHost();
}

void ConnectionHandler::onDisconnected()
{
    emit disconnected(this);
    m_socket->deleteLater();
}

// =======================================================================
// Authentication helpers (v2.2).
// =======================================================================
bool ConnectionHandler::authenticate(const QJsonObject &request, QJsonObject &response,
                                     SessionManager::Session &session)
{
    const QString token = request.value(QLatin1String("token")).toString();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (token.isEmpty() || !m_sessions || !m_sessions->resolve(token, &session, now)) {
        response[QLatin1String("success")] = false;
        response[QLatin1String("error")] = QLatin1String("unauthorized");
        response[QLatin1String("message")] =
            QStringLiteral("Authentication required: send the session token "
                           "returned by a successful login.");
        if (m_stats)
            m_stats->authFailures.fetch_add(1, std::memory_order_relaxed);
        sendJson(response);
        return false;
    }
    return true;
}

void ConnectionHandler::forbid(QJsonObject &response, const char *message)
{
    response[QLatin1String("success")] = false;
    response[QLatin1String("error")] = QLatin1String("forbidden");
    response[QLatin1String("message")] = QString::fromLatin1(message);
    if (m_stats)
        m_stats->authFailures.fetch_add(1, std::memory_order_relaxed);
    sendJson(response);
}

bool ConnectionHandler::orderBelongsToRestaurant(int orderId, int restaurantId)
{
    QSqlQuery details = m_dbHandler->getOrderDetails(orderId);
    return details.next()
           && details.value(QStringLiteral("restaurant_id")).toInt() == restaurantId;
}

// =======================================================================
// Command dispatch.  Every request has a "command" field; every answer
// echoes "type": "response" + "command" + result fields, so clients can
// correlate replies.  See docs/PROTOCOL.md for the full reference.
//
// Auth model (v2.2): `login` is the only unauthenticated command.  It
// mints a session token; everything else must present it.  Admin-only
// management commands, restaurant ownership and per-user wallet access
// are enforced here - a client-sent user_id is never trusted on its own.
// =======================================================================
void ConnectionHandler::processCommand(const QJsonObject &request)
{
    QElapsedTimer requestTimer;
    requestTimer.start();
    dispatchCommand(request);
    LOG_DEBUG("req", QStringLiteral("%1 %2 %3ms")
                        .arg(m_socket ? m_socket->peerAddress().toString()
                                      : QStringLiteral("?"),
                             request.value(QLatin1String("command")).toString()
                                 .left(48))
                        .arg(requestTimer.elapsed()));
}

void ConnectionHandler::dispatchCommand(const QJsonObject &request)
{
    if (!request.contains(QLatin1String("command"))) {
        LOG_WARNING("req", "message without a command field");
        return;
    }

    if (m_stats)
        m_stats->requestsServed.fetch_add(1, std::memory_order_relaxed);

    const QString command = request.value(QLatin1String("command")).toString();
    QJsonObject response;
    response[QLatin1String("type")] = QLatin1String("response");
    response[QLatin1String("command")] = command;

    // ------------------------------ login ------------------------------
    if (command == QLatin1String("login")) {
        const QString username = request.value(QLatin1String("username")).toString();
        const QString password = request.value(QLatin1String("password")).toString();

        QString role;
        int userId = -1;
        const bool success = m_dbHandler->loginUser(username, password, role, userId);
        response[QLatin1String("success")] = success;
        response[QLatin1String("role")] = role;
        response[QLatin1String("user_id")] = userId;
        if (success && m_sessions) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const QString token = m_sessions->openSession(username, role, userId, now);
            response[QLatin1String("session_token")] = token;
            response[QLatin1String("expires_in")] =
                int(m_sessions->lifetimeMs() / 1000);
        }
        sendJson(response);
        return;
    }

    // Everything below this line is authenticated territory.
    SessionManager::Session session;
    if (!authenticate(request, response, session))
        return; // response already sent

    // ------------------------------ logout -----------------------------
    if (command == QLatin1String("logout")) {
        response[QLatin1String("success")] =
            m_sessions->revoke(request.value(QLatin1String("token")).toString());
        sendJson(response);
        return;
    }

    // ----------------------- admin-only commands ------------------------
    const bool isAdmin = session.role == QLatin1String("admin");

    if (command == QLatin1String("get_server_stats")) {
        if (!isAdmin) { forbid(response, "Admin role required."); return; }
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        response[QLatin1String("uptime_seconds")] = int((now - m_stats->startedAtMs) / 1000);
        response[QLatin1String("active_sessions")] = m_sessions->activeCount(now);
        response[QLatin1String("sessions_opened_total")] =
            qlonglong(m_sessions->totalOpened());
        response[QLatin1String("connections_accepted")] =
            qlonglong(m_stats->connectionsAccepted.load());
        response[QLatin1String("tls_connections")] =
            qlonglong(m_stats->tlsConnections.load());
        response[QLatin1String("requests_served")] =
            qlonglong(m_stats->requestsServed.load());
        response[QLatin1String("requests_rejected")] =
            qlonglong(m_stats->requestsRejected.load());
        response[QLatin1String("auth_failures")] =
            qlonglong(m_stats->authFailures.load());
        response[QLatin1String("success")] = true;
        sendJson(response);
    }
    else if (command == QLatin1String("get_users")) {
        if (!isAdmin) { forbid(response, "Admin role required."); return; }
        QJsonArray arr;
        QSqlQuery q = m_dbHandler->readAllUsers();
        while (q.next()) {
            QJsonObject u;
            u[QLatin1String("id")] = q.value(QStringLiteral("id")).toInt();
            u[QLatin1String("username")] = q.value(QStringLiteral("username")).toString();
            u[QLatin1String("role")] = q.value(QStringLiteral("role")).toString();
            u[QLatin1String("is_active")] = q.value(QStringLiteral("is_active")).toBool();
            u[QLatin1String("is_approved")] = q.value(QStringLiteral("is_approved")).toBool();
            arr.append(u);
        }
        response[QLatin1String("data")] = arr;
        sendJson(response);
    }
    else if (command == QLatin1String("block_user")
             || command == QLatin1String("unblock_user")
             || command == QLatin1String("delete_user")
             || command == QLatin1String("approve_restaurant")
             || command == QLatin1String("disapprove_restaurant")) {
        if (!isAdmin) { forbid(response, "Admin role required."); return; }
        const int userId = request.value(QLatin1String("user_id")).toInt();
        bool ok = false;
        if (command == QLatin1String("block_user"))
            ok = m_dbHandler->blockUser(userId);
        else if (command == QLatin1String("unblock_user"))
            ok = m_dbHandler->unblockUser(userId);
        else if (command == QLatin1String("delete_user"))
            ok = m_dbHandler->deleteUser(userId);
        else if (command == QLatin1String("approve_restaurant"))
            ok = m_dbHandler->approveRestaurant(userId);
        else
            ok = m_dbHandler->disapproveRestaurant(userId);
        response[QLatin1String("success")] = ok;
        sendJson(response);
    }
    else if (command == QLatin1String("get_orders")) {
        if (!isAdmin) { forbid(response, "Admin role required."); return; }
        QJsonArray arr;
        QSqlQuery q;
        if (request.contains(QLatin1String("restaurant_id"))) {
            // The restaurant_id filter was silently ignored before.
            q = m_dbHandler->getOrdersForRestaurant(
                request.value(QLatin1String("restaurant_id")).toInt());
        } else {
            q = m_dbHandler->readAllOrders();
        }
        while (q.next()) {
            QJsonObject o;
            o[QLatin1String("id")] = q.value(QStringLiteral("id")).toInt();
            if (q.record().indexOf(QStringLiteral("customer_id")) >= 0)
                o[QLatin1String("customer_id")] = q.value(QStringLiteral("customer_id")).toInt();
            o[QLatin1String("restaurant_id")] = q.value(QStringLiteral("restaurant_id")).toInt();
            o[QLatin1String("status")] = q.value(QStringLiteral("status")).toString();
            o[QLatin1String("total_price")] = q.value(QStringLiteral("total_price")).toDouble();
            o[QLatin1String("created_at")] = q.value(QStringLiteral("created_at")).toString();
            arr.append(o);
        }
        response[QLatin1String("data")] = arr;
        sendJson(response);
    }
    else if (command == QLatin1String("get_reviews")) {
        if (!isAdmin) { forbid(response, "Admin role required."); return; }
        QJsonArray arr;
        QSqlQuery q = m_dbHandler->getReviews();
        while (q.next()) {
            QJsonObject r;
            r[QLatin1String("order_id")] = q.value(QStringLiteral("order_id")).toInt();
            r[QLatin1String("restaurant_id")] = q.value(QStringLiteral("restaurant_id")).toInt();
            r[QLatin1String("restaurant_name")] = q.value(QStringLiteral("restaurant_name")).toString();
            r[QLatin1String("customer_name")] = q.value(QStringLiteral("customer_name")).toString();
            r[QLatin1String("rating")] = q.value(QStringLiteral("rating")).toInt();
            r[QLatin1String("comment")] = q.value(QStringLiteral("comment")).toString();
            r[QLatin1String("created_at")] = q.value(QStringLiteral("created_at")).toString();
            arr.append(r);
        }
        response[QLatin1String("data")] = arr;
        sendJson(response);
    }

    // ----------------------------- coupons ------------------------------
    else if (command == QLatin1String("get_coupons")
             || command == QLatin1String("create_coupon")
             || command == QLatin1String("toggle_coupon")) {
        if (!isAdmin) { forbid(response, "Admin role required."); return; }
        if (command == QLatin1String("get_coupons")) {
            QJsonArray arr;
            QSqlQuery q = m_dbHandler->getAllCoupons();
            while (q.next()) {
                QJsonObject c;
                c[QLatin1String("id")] = q.value(QStringLiteral("id")).toInt();
                c[QLatin1String("code")] = q.value(QStringLiteral("code")).toString();
                c[QLatin1String("kind")] = q.value(QStringLiteral("kind")).toString();
                c[QLatin1String("value")] = q.value(QStringLiteral("value")).toDouble();
                c[QLatin1String("min_order")] = q.value(QStringLiteral("min_order")).toDouble();
                c[QLatin1String("max_uses")] = q.value(QStringLiteral("max_uses")).toInt();
                c[QLatin1String("used_count")] = q.value(QStringLiteral("used_count")).toInt();
                c[QLatin1String("is_active")] = q.value(QStringLiteral("is_active")).toInt() == 1;
                c[QLatin1String("expires_at")] = q.value(QStringLiteral("expires_at")).toString();
                arr.append(c);
            }
            response[QLatin1String("data")] = arr;
        } else if (command == QLatin1String("create_coupon")) {
            response[QLatin1String("success")] = m_dbHandler->createCoupon(
                request.value(QLatin1String("code")).toString(),
                request.value(QLatin1String("kind")).toString(),
                request.value(QLatin1String("value")).toDouble(),
                request.value(QLatin1String("min_order")).toDouble(),
                request.value(QLatin1String("expires_at")).toString(),
                request.value(QLatin1String("max_uses")).toInt());
        } else {
            response[QLatin1String("success")] = m_dbHandler->setCouponActive(
                request.value(QLatin1String("coupon_id")).toInt(),
                request.value(QLatin1String("active")).toBool());
        }
        sendJson(response);
    }

    // Read-only coupon check (checkout dialog) - any authenticated session.
    else if (command == QLatin1String("validate_coupon")) {
        const CouponResult r = m_dbHandler->validateCoupon(
            request.value(QLatin1String("code")).toString(),
            request.value(QLatin1String("total")).toDouble());
        response[QLatin1String("ok")] = r.ok;
        response[QLatin1String("discount")] = r.discount;
        response[QLatin1String("coupon_id")] = r.couponId;
        response[QLatin1String("message")] = r.message;
        sendJson(response);
    }

    // ------------------------ restaurant commands -----------------------
    else if (command == QLatin1String("add_menu_item")) {
        const int restaurantId = request.value(QLatin1String("restaurant_id")).toInt();
        if (session.role != QLatin1String("restaurant") || restaurantId != session.userId) { forbid(response, "You can only manage your own restaurant's menu."); return; }
        response[QLatin1String("success")] = m_dbHandler->addFoodItem(
            restaurantId,
            request.value(QLatin1String("name")).toString(),
            request.value(QLatin1String("price")).toDouble(),
            request.value(QLatin1String("description")).toString());
        sendJson(response);
    }
    else if (command == QLatin1String("update_menu_item")
             || command == QLatin1String("delete_menu_item")) {
        if (session.role != QLatin1String("restaurant")) { forbid(response, "Restaurant role required."); return; }
        const int foodId = request.value(QLatin1String("id")).toInt();
        bool owned = false;
        const QVector<FoodItem> menu = m_dbHandler->getMenuForRestaurant(session.userId);
        for (const FoodItem &item : menu)
            if (item.id == foodId) { owned = true; break; }
        if (!owned) { forbid(response, "This menu item does not belong to your restaurant."); return; }
        if (command == QLatin1String("update_menu_item"))
            response[QLatin1String("success")] = m_dbHandler->updateFoodItem(
                foodId,
                request.value(QLatin1String("name")).toString(),
                request.value(QLatin1String("price")).toDouble(),
                request.value(QLatin1String("description")).toString());
        else
            response[QLatin1String("success")] = m_dbHandler->deleteFoodItem(foodId);
        sendJson(response);
    }
    else if (command == QLatin1String("get_orders_for_restaurant")) {
        const int restaurantId = request.value(QLatin1String("restaurant_id")).toInt();
        if (session.role != QLatin1String("restaurant") || restaurantId != session.userId) { forbid(response, "You can only read your own restaurant's orders."); return; }
        QJsonArray data;
        QSqlQuery query = m_dbHandler->getOrdersForRestaurant(restaurantId);
        while (query.next()) {
            QJsonObject order;
            order[QLatin1String("id")] = query.value(QStringLiteral("id")).toInt();
            order[QLatin1String("customer_username")] = query.value(QStringLiteral("username")).toString();
            order[QLatin1String("status")] = query.value(QStringLiteral("status")).toString();
            order[QLatin1String("total_price")] = query.value(QStringLiteral("total_price")).toDouble();
            order[QLatin1String("created_at")] = query.value(QStringLiteral("created_at")).toString();
            data.append(order);
        }
        response[QLatin1String("success")] = true;
        response[QLatin1String("data")] = data;
        sendJson(response);
    }
    else if (command == QLatin1String("update_order_status")) {
        const int orderId = request.value(QLatin1String("order_id")).toInt();
        const QString newStatus = request.value(QLatin1String("new_status")).toString();

        // Ownership: admins may steer any order; a restaurant only its
        // own; a customer only confirms/cancels an order it placed.
        bool allowed = isAdmin;
        if (!allowed && session.role == QLatin1String("restaurant"))
            allowed = orderBelongsToRestaurant(orderId, session.userId);
        if (!allowed && session.role == QLatin1String("customer")) {
            QSqlQuery details = m_dbHandler->getOrderDetails(orderId);
            allowed = details.next()
                      && details.value(QStringLiteral("customer_id")).toInt() == session.userId;
        }
        if (!allowed) { forbid(response, "This order does not belong to your session."); return; }

        response[QLatin1String("success")] = m_dbHandler->updateOrderStatus(orderId, newStatus);
        sendJson(response);
        if (response.value(QLatin1String("success")).toBool())
            emit orderStatusChanged(orderId, newStatus, this);
    }

    // ------------------------- customer commands ------------------------
    else if (command == QLatin1String("get_wallet_balance")
             || command == QLatin1String("charge_wallet")
             || command == QLatin1String("pay_from_wallet")) {
        const int userId = request.value(QLatin1String("user_id")).toInt();
        if (userId != session.userId) { forbid(response, "Wallet access is limited to the session's own user."); return; }
        if (command == QLatin1String("get_wallet_balance")) {
            response[QLatin1String("balance")] = m_dbHandler->getWalletBalance(userId);
        } else if (command == QLatin1String("charge_wallet")) {
            response[QLatin1String("success")] = m_dbHandler->chargeWallet(
                userId,
                request.value(QLatin1String("amount")).toDouble(),
                request.value(QLatin1String("note")).toString());
        } else {
            response[QLatin1String("success")] = m_dbHandler->payFromWallet(
                userId,
                request.value(QLatin1String("amount")).toDouble(),
                request.value(QLatin1String("order_id")).toInt());
        }
        sendJson(response);
    }
    else if (command == QLatin1String("place_order")) {
        // The session, not the client, decides whose order this is.
        if (request.value(QLatin1String("customer_id")).toInt() != session.userId) {
            forbid(response, "Orders can only be placed for the signed-in customer.");
            return;
        }

        // The server (not the client) assigns the id and timestamps.
        QJsonObject orderData = request;
        orderData.remove(QLatin1String("command"));
        orderData.remove(QLatin1String("token"));
        orderData[QLatin1String("status")] = QString(OrderStatus::Pending);
        orderData[QLatin1String("created_at")] =
            QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));

        // Optional coupon: validated + consumed server-side, the discount
        // is stored with the order so every dashboard sees the same number.
        const QString couponCode = request.value(QLatin1String("coupon_code")).toString().trimmed();
        double total = orderData.value(QLatin1String("total_price")).toDouble();
        if (!couponCode.isEmpty()) {
            const CouponResult coupon = m_dbHandler->validateCoupon(couponCode, total);
            if (!coupon.ok) {
                response[QLatin1String("success")] = false;
                response[QLatin1String("error")] = coupon.message;
                sendJson(response);
                return;
            }
            orderData[QLatin1String("discount")] = coupon.discount;
            orderData[QLatin1String("coupon_code")] = couponCode;
            total -= coupon.discount;
        }
        orderData[QLatin1String("total_price")] = total;

        const int orderId = m_dbHandler->createNewOrder(orderData);
        if (orderId <= 0) {
            response[QLatin1String("success")] = false;
            sendJson(response);
            return;
        }

        QMap<int, CartItem> items;
        const QJsonArray itemsJson = request.value(QLatin1String("items")).toArray();
        for (const QJsonValue &val : itemsJson) {
            const QJsonObject obj = val.toObject();
            CartItem item;
            item.foodData = obj;
            item.quantity = obj.value(QLatin1String("quantity")).toInt();
            items.insert(obj.value(QLatin1String("id")).toInt(), item);
        }

        const bool itemsSaved = m_dbHandler->addOrderItems(orderId, items);
        response[QLatin1String("success")] = itemsSaved;
        response[QLatin1String("order_id")] = orderId;
        response[QLatin1String("total_price")] = total;

        // Wallet payment: atomic deduction, the order is marked cancelled
        // when the balance turned out to be insufficient.
        if (itemsSaved && orderData.value(QLatin1String("payment_method")).toString() == QLatin1String("wallet")) {
            if (!m_dbHandler->payFromWallet(session.userId, total, orderId)) {
                m_dbHandler->updateOrderStatus(orderId, QString(OrderStatus::Cancelled));
                response[QLatin1String("success")] = false;
                response[QLatin1String("error")] = QStringLiteral("موجودی کیف پول کافی نیست.");
            } else if (!couponCode.isEmpty()) {
                const int couponId = m_dbHandler->findCouponId(couponCode);
                if (couponId > 0)
                    m_dbHandler->consumeCoupon(couponId);
            }
        } else if (itemsSaved && !couponCode.isEmpty()) {
            const int couponId = m_dbHandler->findCouponId(couponCode);
            if (couponId > 0)
                m_dbHandler->consumeCoupon(couponId);
        }
        sendJson(response);
    }
    else if (command == QLatin1String("submit_review")) {
        const int orderId = request.value(QLatin1String("order_id")).toInt();

        // Only the customer who received the order may review it.
        QSqlQuery details = m_dbHandler->getOrderDetails(orderId);
        if (!details.next()
            || details.value(QStringLiteral("customer_id")).toInt() != session.userId) {
            forbid(response, "You can only review your own orders.");
            return;
        }

        int restaurantId = details.value(QStringLiteral("restaurant_id")).toInt();
        const int customerId = session.userId;

        const bool ok = m_dbHandler->submitReview(
            orderId, restaurantId, customerId,
            qBound(1, request.value(QLatin1String("rating")).toInt(), 5),
            request.value(QLatin1String("review_text")).toString());
        response[QLatin1String("success")] = ok;
        sendJson(response);
    }

    // ---------------------- shared / profile commands --------------------
    else if (command == QLatin1String("get_restaurants")) {
        const QString name = request.value(QLatin1String("name")).toString();
        const QString type = request.value(QLatin1String("type")).toString();
        const QString location = request.value(QLatin1String("location")).toString();

        QJsonArray data;
        QSqlQuery query = m_dbHandler->getAllRestaurants(type, location, name);
        while (query.next()) {
            QJsonObject obj;
            obj[QLatin1String("id")] = query.value(QStringLiteral("id")).toInt();
            obj[QLatin1String("name")] = query.value(QStringLiteral("name")).toString();
            obj[QLatin1String("type")] = query.value(QStringLiteral("type")).toString();
            obj[QLatin1String("location")] = query.value(QStringLiteral("location")).toString();
            obj[QLatin1String("price_range")] = query.value(QStringLiteral("price_range")).toInt();
            data.append(obj);
        }

        response[QLatin1String("type")] = QLatin1String("restaurant_list_response");
        response[QLatin1String("data")] = data;
        sendJson(response);
    }
    else if (command == QLatin1String("get_menu_items_for_restaurant")) {
        const int restaurantId = request.value(QLatin1String("restaurant_id")).toInt();
        // Admins browse every menu; restaurants only their own; customers
        // may read menus to fill their cart.
        const bool allowed = isAdmin
                             || session.role == QLatin1String("customer")
                             || restaurantId == session.userId;
        if (!allowed) { forbid(response, "You can only read your own restaurant's menu."); return; }
        QJsonArray data;
        const QVector<FoodItem> menu = m_dbHandler->getMenuForRestaurant(restaurantId);
        for (const FoodItem &item : menu) {
            QJsonObject obj;
            obj[QLatin1String("id")] = item.id;
            obj[QLatin1String("name")] = item.name;
            obj[QLatin1String("price")] = item.price;
            obj[QLatin1String("description")] = item.description;
            data.append(obj);
        }
        response[QLatin1String("success")] = true;
        response[QLatin1String("data")] = data;
        sendJson(response);
    }
    else if (command == QLatin1String("update_profile")) {
        const int userId = request.value(QLatin1String("user_id")).toInt();
        if (userId != session.userId) { forbid(response, "Profiles can only be edited by their owner."); return; }
        const QJsonObject data = request.value(QLatin1String("data")).toObject();
        const bool ok = m_dbHandler->updateUserDetails(
            userId,
            data.value(QLatin1String("username")).toString(),
            data.value(QLatin1String("address")).toString());
        response[QLatin1String("success")] = ok;
        sendJson(response);
    }

    // ---------------------------- broadcasts -----------------------------
    else if (command == QLatin1String("notify_order_placed")) {
        // Clients write the order to the shared database themselves; this
        // command only tells the other sessions about it.  (The old flow
        // re-inserted the order through place_order and duplicated rows.)
        QJsonObject note;
        note[QLatin1String("type")] = QLatin1String("new_order_notification");
        QJsonObject data;
        for (const QString &key : { QStringLiteral("order_id"), QStringLiteral("restaurant_id"),
                                    QStringLiteral("total_price") })
            data[key] = request.value(key);
        // Stamped by the server from the authenticated session - clients
        // cannot spoof someone else's name into restaurant dashboards.
        data[QLatin1String("customer")] = session.username;
        note[QLatin1String("data")] = data;
        emit broadcastRequested(this, note); // no direct reply
    }

    else if (command == QLatin1String("send_chat_message")) {
        QJsonObject chatMsg;
        chatMsg[QLatin1String("type")] = QLatin1String("new_chat_message_notification");
        QJsonObject payload = request;
        payload.remove(QLatin1String("token"));
        chatMsg[QLatin1String("data")] = payload;
        emit broadcastRequested(this, chatMsg); // no direct reply
    }

    // --------------------------- unknown command -------------------------
    else {
        response[QLatin1String("error")] = QStringLiteral("Unknown command");
        sendJson(response);
    }
}
