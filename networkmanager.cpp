#include "networkmanager.h"
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
        // ... سایر انواع پاسخ‌ها و اعلان‌ها ...
    }
}
