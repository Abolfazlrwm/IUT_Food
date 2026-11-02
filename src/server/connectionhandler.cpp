#include "connectionhandler.h"
#include "databasehandler.h"
#include "orderstatus.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlRecord>

ConnectionHandler::ConnectionHandler(qintptr socketDescriptor, DataBaseHandler *dbHandler,
                                     QObject *parent)
    : QObject(parent), m_dbHandler(dbHandler)
{
    m_socket = new QTcpSocket(this);
    m_socket->setSocketDescriptor(socketDescriptor);

    connect(m_socket, &QTcpSocket::readyRead, this, &ConnectionHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ConnectionHandler::onDisconnected);

    qDebug() << "New client connected:" << socketDescriptor;
}

ConnectionHandler::~ConnectionHandler()
{
    qDebug() << "ConnectionHandler destroyed";
}

void ConnectionHandler::sendJson(const QJsonObject &obj)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
    m_socket->write(payload);
    m_socket->flush();
}

void ConnectionHandler::onReadyRead()
{
    while (m_socket->canReadLine()) {
        const QByteArray line = m_socket->readLine().trimmed();
        if (line.isEmpty())
            continue;

        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) {
            qDebug() << "Invalid JSON from client.";
            continue;
        }
        processCommand(doc.object());
    }
}

void ConnectionHandler::onDisconnected()
{
    emit disconnected(this);
    m_socket->deleteLater();
}

// =======================================================================
// Command dispatch.  Every request has a "command" field; every answer
// echoes "type": "response" + "command" + result fields, so clients can
// correlate replies.  See docs/PROTOCOL.md for the full reference.
// =======================================================================
void ConnectionHandler::processCommand(const QJsonObject &request)
{
    if (!request.contains(QLatin1String("command"))) {
        qDebug() << "No command field in request.";
        return;
    }

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
        sendJson(response);
    }

    // -------------------------- get restaurants ------------------------
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

    // ---------------------------- place order --------------------------
    else if (command == QLatin1String("place_order")) {
        // The server (not the client) assigns the id and timestamps.
        QJsonObject orderData = request;
        orderData.remove(QLatin1String("command"));
        orderData[QLatin1String("status")] = QString(OrderStatus::Pending);
        orderData[QLatin1String("created_at")] =
            QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));

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
        sendJson(response);
    }

    // --------------------------- update profile ------------------------
    else if (command == QLatin1String("update_profile")) {
        const int userId = request.value(QLatin1String("user_id")).toInt();
        const QJsonObject data = request.value(QLatin1String("data")).toObject();
        const bool ok = m_dbHandler->updateUserDetails(
            userId,
            data.value(QLatin1String("username")).toString(),
            data.value(QLatin1String("address")).toString());
        response[QLatin1String("success")] = ok;
        sendJson(response);
    }

    // ---------------------------- chat message --------------------------
    else if (command == QLatin1String("send_chat_message")) {
        QJsonObject chatMsg;
        chatMsg[QLatin1String("type")] = QLatin1String("new_chat_message_notification");
        chatMsg[QLatin1String("data")] = request;
        emit broadcastRequested(this, chatMsg); // no direct reply
    }

    // ---------------------------- submit review -------------------------
    else if (command == QLatin1String("submit_review")) {
        const int orderId = request.value(QLatin1String("order_id")).toInt();
        int restaurantId = -1;
        int customerId = -1;

        QSqlQuery details = m_dbHandler->getOrderDetails(orderId);
        if (details.next()) {
            restaurantId = details.value(QStringLiteral("restaurant_id")).toInt();
            customerId = details.value(QStringLiteral("customer_id")).toInt();
        }

        const bool ok = m_dbHandler->submitReview(
            orderId, restaurantId, customerId,
            qBound(1, request.value(QLatin1String("rating")).toInt(), 5),
            request.value(QLatin1String("review_text")).toString());
        response[QLatin1String("success")] = ok;
        sendJson(response);
    }

    // --------------------------- admin commands -------------------------
    else if (command == QLatin1String("block_user")) {
        response[QLatin1String("success")] =
            m_dbHandler->blockUser(request.value(QLatin1String("user_id")).toInt());
        sendJson(response);
    }
    else if (command == QLatin1String("unblock_user")) {
        response[QLatin1String("success")] =
            m_dbHandler->unblockUser(request.value(QLatin1String("user_id")).toInt());
        sendJson(response);
    }
    else if (command == QLatin1String("approve_restaurant")) {
        response[QLatin1String("success")] =
            m_dbHandler->approveRestaurant(request.value(QLatin1String("user_id")).toInt());
        sendJson(response);
    }
    else if (command == QLatin1String("disapprove_restaurant")) {
        response[QLatin1String("success")] =
            m_dbHandler->disapproveRestaurant(request.value(QLatin1String("user_id")).toInt());
        sendJson(response);
    }
    else if (command == QLatin1String("delete_user")) {
        response[QLatin1String("success")] =
            m_dbHandler->deleteUser(request.value(QLatin1String("user_id")).toInt());
        sendJson(response);
    }
    else if (command == QLatin1String("get_users")) {
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
    else if (command == QLatin1String("get_orders")) {
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

    // ------------------------ restaurant commands -----------------------
    else if (command == QLatin1String("add_menu_item")) {
        response[QLatin1String("success")] = m_dbHandler->addFoodItem(
            request.value(QLatin1String("restaurant_id")).toInt(),
            request.value(QLatin1String("name")).toString(),
            request.value(QLatin1String("price")).toDouble(),
            request.value(QLatin1String("description")).toString());
        sendJson(response);
    }
    else if (command == QLatin1String("update_menu_item")) {
        response[QLatin1String("success")] = m_dbHandler->updateFoodItem(
            request.value(QLatin1String("id")).toInt(),
            request.value(QLatin1String("name")).toString(),
            request.value(QLatin1String("price")).toDouble(),
            request.value(QLatin1String("description")).toString());
        sendJson(response);
    }
    else if (command == QLatin1String("delete_menu_item")) {
        response[QLatin1String("success")] =
            m_dbHandler->deleteFoodItem(request.value(QLatin1String("id")).toInt());
        sendJson(response);
    }
    else if (command == QLatin1String("get_menu_items_for_restaurant")) {
        QJsonArray data;
        const QVector<FoodItem> menu =
            m_dbHandler->getMenuForRestaurant(request.value(QLatin1String("restaurant_id")).toInt());
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
    else if (command == QLatin1String("get_orders_for_restaurant")) {
        QJsonArray data;
        QSqlQuery query = m_dbHandler->getOrdersForRestaurant(
            request.value(QLatin1String("restaurant_id")).toInt());
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
        response[QLatin1String("success")] = m_dbHandler->updateOrderStatus(orderId, newStatus);
        sendJson(response);
        if (response.value(QLatin1String("success")).toBool())
            emit orderStatusChanged(orderId, newStatus, this);
    }

    // --------------------------- unknown command -------------------------
    else {
        response[QLatin1String("error")] = QStringLiteral("Unknown command");
        sendJson(response);
    }
}
