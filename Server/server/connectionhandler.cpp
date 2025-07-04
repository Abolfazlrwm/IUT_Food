#include "ConnectionHandler.h"
#include <QJsonDocument>
#include <QSqlQuery>
#include <QJsonArray>
#include <QDebug>

ConnectionHandler::ConnectionHandler(qintptr socketDescriptor, DataBaseHandler* dbHandler, QObject *parent)
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

void ConnectionHandler::sendJson(const QJsonObject& obj)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    QJsonDocument doc(obj);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(data);
    m_socket->flush();
}

void ConnectionHandler::onReadyRead()
{
    while (m_socket->canReadLine()) {
        QByteArray line = m_socket->readLine().trimmed();
        QJsonDocument doc = QJsonDocument::fromJson(line);

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

// ============= MAIN LOGIC =============
void ConnectionHandler::processCommand(const QJsonObject& request)
{
    if (!request.contains("command")) {
        qDebug() << "No command field in request.";
        return;
    }

    QString command = request["command"].toString();
    QJsonObject response;
    response["type"] = "response";
    response["command"] = command;

    // ---------------------- LOGIN ----------------------
    if (command == "login") {
        QString username = request["username"].toString();
        QString password = request["password"].toString();

        bool success = m_dbHandler->loginUser(username, password);
        QString role = m_dbHandler->getUserRole(username);

        response["success"] = success;
        response["role"] = role;
        sendJson(response);
    }

    // ------------------- GET RESTAURANTS -------------------
    else if (command == "get_restaurants") {
        QString name = request.value("name").toString();
        QString type = request.value("type").toString();
        QString location = request.value("location").toString();

        QJsonArray data;
        QSqlQuery query = m_dbHandler->getAllRestaurants(type, location, name);
        while (query.next()) {
            QJsonObject obj;
            obj["id"] = query.value("id").toInt();
            obj["name"] = query.value("name").toString();
            obj["type"] = query.value("type").toString();
            obj["location"] = query.value("location").toString();
            obj["price_range"] = query.value("price_range").toInt();
            data.append(obj);
        }

        response["type"] = "restaurant_list_response";
        response["data"] = data;
        sendJson(response);
    }

    // ------------------- PLACE ORDER -------------------
    else if (command == "place_order") {
        QJsonObject orderData = request;
        orderData.remove("command");

        bool orderSaved = m_dbHandler->createNewOrder(orderData);
        if (!orderSaved) {
            response["success"] = false;
            sendJson(response);
            return;
        }

        int orderId = orderData["id"].toInt();
        QJsonArray itemsJson = request["items"].toArray();
        QMap<int, CartItem> items;

        for (const QJsonValue& val : itemsJson) {
            QJsonObject obj = val.toObject();
            CartItem item;
            item.foodData = obj;
            item.quantity = obj["quantity"].toInt();
            int foodId = obj["id"].toInt();
            items[foodId] = item;
        }

        bool itemsSaved = m_dbHandler->addOrderItems(orderId, items);
        response["success"] = itemsSaved;
        sendJson(response);
    }

    // ------------------- UPDATE PROFILE -------------------
    else if (command == "update_profile") {
        int userId = request["user_id"].toInt();
        QJsonObject data = request["data"].toObject();
        QString newUsername = data["username"].toString();
        QString newAddress = data["address"].toString();

        bool ok = m_dbHandler->updateUserDetails(userId, newUsername, newAddress);
        response["success"] = ok;
        sendJson(response);
    }

    // ------------------- CHAT MESSAGE -------------------
    else if (command == "send_chat_message") {
        int orderId = request["order_id"].toInt();
        QString sender = request["sender"].toString();

        QJsonObject chatMsg;
        chatMsg["type"] = "new_chat_message_notification";
        chatMsg["data"] = request;

        // Inform server to chat with specefic users
        emit newMessageBroadcast(orderId, chatMsg);
    }


    // ------------------- SUBMIT REVIEW -------------------
    else if (command == "submit_review") {
        int orderId = request["order_id"].toInt();

        bool ok = m_dbHandler->markOrderAsReviewed(orderId);
        response["success"] = ok;
        sendJson(response);
    }
    // ------------------- ADMIN COMMANDS -------------------
    else if (command == "block_user") {
        int userId = request["user_id"].toInt();
        bool ok = m_dbHandler->blockUser(userId);
        response["success"] = ok;
        sendJson(response);
    }
    else if (command == "unblock_user") {
        int userId = request["user_id"].toInt();
        bool ok = m_dbHandler->unblockUser(userId);
        response["success"] = ok;
        sendJson(response);
    }
    else if (command == "approve_restaurant") {
        int userId = request["user_id"].toInt();
        bool ok = m_dbHandler->approveRestaurant(userId);
        response["success"] = ok;
        sendJson(response);
    }
    else if (command == "disapprove_restaurant") {
        int userId = request["user_id"].toInt();
        bool ok = m_dbHandler->disapproveRestaurant(userId);
        response["success"] = ok;
        sendJson(response);
    }
    else if (command == "get_users") {
        QSqlQuery q = m_dbHandler->readAllUsers();
        QJsonArray arr;
        while (q.next()) {
            QJsonObject u;
            u["id"] = q.value("id").toInt();
            u["username"] = q.value("username").toString();
            u["role"] = q.value("role").toString();
            u["is_active"] = q.value("is_active").toBool();
            u["is_approved"] = q.value("is_approved").toBool();
            arr.append(u);
        }
        response["data"] = arr;
        sendJson(response);
    }
    else if (command == "get_orders") {
        QSqlQuery q = m_dbHandler->readAllOrders();
        QJsonArray arr;
        while (q.next()) {
            QJsonObject o;
            o["id"] = q.value("id").toInt();
            o["customer_id"] = q.value("customer_id").toInt();
            o["restaurant_id"] = q.value("restaurant_id").toInt();
            o["status"] = q.value("status").toString();
            o["total_price"] = q.value("total_price").toDouble();
            o["created_at"] = q.value("created_at").toString();
            arr.append(o);
        }
        response["data"] = arr;
        sendJson(response);
    }
    else if (command == "delete_user") {
        int userId = request["user_id"].toInt();
        bool ok = m_dbHandler->deleteUser(userId);
        response["success"] = ok;
        sendJson(response);
    }

    // RESTAURANT
    else if (command == "add_menu_item") {
        int restaurantId = request["restaurant_id"].toInt();
        QString name = request["name"].toString();
        double price = request["price"].toDouble();
        QString description = request["description"].toString();

        bool ok = m_dbHandler->addFoodItem(restaurantId, name, price, description);
        response["success"] = ok;
        sendJson(response);
    }
    else if (command == "update_menu_item") {
        int foodId = request["id"].toInt();
        QString name = request["name"].toString();
        double price = request["price"].toDouble();
        QString description = request["description"].toString();

        bool ok = m_dbHandler->updateFoodItem(foodId, name, price, description);
        response["success"] = ok;
        sendJson(response);
    }
    else if (command == "delete_menu_item") {
        int foodId = request["id"].toInt();

        bool ok = m_dbHandler->deleteFoodItem(foodId);
        response["success"] = ok;
        sendJson(response);
    }

    else if (command == "get_menu_items_for_restaurant") {
        int restaurantId = request["restaurant_id"].toInt();

        QJsonArray data;
        QSqlQuery query = m_dbHandler->getMenuItemsForRestaurant(restaurantId);
        while (query.next()) {
            QJsonObject item;
            item["id"] = query.value("id").toInt();
            item["name"] = query.value("name").toString();
            item["price"] = query.value("price").toDouble();
            item["description"] = query.value("description").toString();
            data.append(item);
        }

        response["success"] = true;
        response["data"] = data;
        sendJson(response);
    }

    else if (command == "get_orders_for_restaurant") {
        int restaurantId = request["restaurant_id"].toInt();

        QJsonArray orders;
        QSqlQuery query = m_dbHandler->getOrdersForRestaurant(restaurantId);
        while (query.next()) {
            QJsonObject order;
            order["id"] = query.value("id").toInt();
            order["customer_username"] = query.value("username").toString();
            order["status"] = query.value("status").toString();
            order["total_price"] = query.value("total_price").toDouble();
            order["created_at"] = query.value("created_at").toString();
            orders.append(order);
        }

        response["success"] = true;
        response["data"] = orders;
        sendJson(response);
    }

    else if (command == "update_order_status") {
        int orderId = request["order_id"].toInt();
        QString newStatus = request["new_status"].toString();

        bool ok = m_dbHandler->updateOrderStatus(orderId, newStatus);
        response["success"] = ok;
        sendJson(response);
    }




    // ------------------- UNKNOWN COMMAND -------------------
    else {
        response["error"] = "Unknown command";
        sendJson(response);
    }
}

bool ConnectionHandler::hasOrder(int orderId) const {
    return m_currentOrderId == orderId;
}
