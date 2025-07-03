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


    // ------------------- UNKNOWN COMMAND -------------------
    else {
        response["error"] = "Unknown command";
        sendJson(response);
    }
}

bool ConnectionHandler::hasOrder(int orderId) const {
    return m_currentOrderId == orderId;
}
