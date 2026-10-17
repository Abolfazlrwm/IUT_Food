#include "networkmanager.h"

#include <QJsonDocument>

NetworkManager &NetworkManager::instance()
{
    static NetworkManager instance;
    return instance;
}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::disconnected);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit connectionError(m_socket->errorString());
    });
}

void NetworkManager::connectToServer(const QString &host, quint16 port)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        return;
    if (m_socket->state() == QAbstractSocket::ConnectingState)
        return;
    m_socket->connectToHost(host, port);
}

void NetworkManager::disconnectFromServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        m_socket->disconnectFromHost();
}

bool NetworkManager::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void NetworkManager::sendJson(const QJsonObject &jsonObj)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    const QByteArray payload =
        QJsonDocument(jsonObj).toJson(QJsonDocument::Compact) + '\n';
    m_socket->write(payload);
    m_socket->flush();
}

void NetworkManager::onReadyRead()
{
    // The server replies are '\n'-terminated; buffer partial reads.
    while (m_socket->canReadLine()) {
        const QByteArray line = m_socket->readLine().trimmed();
        if (line.isEmpty())
            continue;

        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject())
            continue;

        const QJsonObject obj = doc.object();
        emit jsonReceived(obj);

        const QString type = obj.value(QLatin1String("type")).toString();
        if (type == QLatin1String("restaurant_list_response")) {
            emit restaurantsReceived(obj.value(QLatin1String("data")).toArray());
        } else if (type == QLatin1String("order_status_update_notification")) {
            emit orderStatusUpdated(obj.value(QLatin1String("data")).toObject());
        } else if (type == QLatin1String("new_chat_message_notification")) {
            emit chatMessageReceived(obj.value(QLatin1String("data")).toObject());
        } else if (type == QLatin1String("response")) {
            emit operationResult(obj.value(QLatin1String("command")).toString(),
                                 obj.value(QLatin1String("success")).toBool(),
                                 obj.value(QLatin1String("message")).toString());
        }
    }
}
