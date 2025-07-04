#include "networkmanager.h"
#include "profilepanel.h"
#include <QJsonDocument>

NetworkManager* NetworkManager::instance = nullptr;

NetworkManager* NetworkManager::getInstance() {
    if (instance == nullptr)
        instance = new NetworkManager();
    return instance;
}

NetworkManager::NetworkManager(QObject *parent) : QObject(parent) {
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::disconnected);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &NetworkManager::errorOccurred);
}

void NetworkManager::connectToServer(const QString& ip, quint16 port) {
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        m_socket->connectToHost(ip, port);
    }
}

void NetworkManager::sendJson(const QJsonObject& jsonObj) {
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(QJsonDocument(jsonObj).toJson(QJsonDocument::Compact));
        m_socket->flush(); // اطمینان از ارسال فوری داده
    }
}

void NetworkManager::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    if (obj.contains("type")) {
        QString messageType = obj["type"].toString();

        if (messageType == "login_response") {
            emit loginResponse(obj["success"].toBool(), obj["message"].toString());
        } else if (messageType == "restaurant_list_response") {
            emit restaurantsReceived(obj["data"].toArray());
        } else if (messageType == "order_status_update_notification") {
            emit orderStatusUpdated(obj["data"].toObject());
        }
        else if(messageType == "new_chat_message_notification"){
            emit newMessageReceived(obj["data"].toObject());
        }

        else if (messageType == "menu_response") {
            emit menuReceived(obj["data"].toArray());
        }
        else if (messageType == "orders_response") {
            emit ordersReceived(obj["data"].toArray());
        }
        else if (messageType == "operation_result") {
            bool success = obj["success"].toBool();
            QString msg = obj["message"].toString();
            emit operationResult(success, msg);
        }

        // ... سایر انواع پاسخ‌ها و اعلان‌ها ...
    }
}

void NetworkManager::requestMenu(int restaurantId) {
    QJsonObject req;
    req["command"] = "get_menu";
    req["restaurant_id"] = restaurantId;
    sendJson(req);
}

void NetworkManager::requestOrders(int restaurantId) {
    QJsonObject req;
    req["command"] = "get_orders";
    req["restaurant_id"] = restaurantId;
    sendJson(req);
}

void NetworkManager::addFoodItem(int restaurantId, const QString& name, double price, const QString& description) {
    QJsonObject req;
    req["command"] = "add_food";
    req["restaurant_id"] = restaurantId;
    req["name"] = name;
    req["price"] = price;
    req["description"] = description;
    sendJson(req);
}

void NetworkManager::editFoodItem(int foodId, const QString& name, double price, const QString& description) {
    QJsonObject req;
    req["command"] = "edit_food";
    req["food_id"] = foodId;
    req["name"] = name;
    req["price"] = price;
    req["description"] = description;
    sendJson(req);
}

void NetworkManager::deleteFoodItem(int foodId) {
    QJsonObject req;
    req["command"] = "delete_food";
    req["food_id"] = foodId;
    sendJson(req);
}

void NetworkManager::updateOrderStatus(int orderId, const QString& newStatus) {
    QJsonObject req;
    req["command"] = "update_order_status";
    req["order_id"] = orderId;
    req["status"] = newStatus;
    sendJson(req);
}
