#include "server.h"
#include "connectionhandler.h"

#include <QDebug>

Server::Server(DataBaseHandler *dbHandler, QObject *parent)
    : QTcpServer(parent), m_dbHandler(dbHandler)
{
}

void Server::incomingConnection(qintptr socketDescriptor)
{
    auto *handler = new ConnectionHandler(socketDescriptor, m_dbHandler, this);
    connect(handler, &ConnectionHandler::disconnected,
            this, &Server::onClientDisconnected);
    connect(handler, &ConnectionHandler::broadcastRequested,
            this, &Server::onBroadcastRequested);
    connect(handler, &ConnectionHandler::orderStatusChanged,
            this, &Server::onStatusBroadcast);

    m_clients.append(handler);
    qDebug() << "Client connected, descriptor:" << socketDescriptor
             << "| total clients:" << m_clients.size();
}

void Server::onClientDisconnected(ConnectionHandler *handler)
{
    m_clients.removeAll(handler);
    handler->deleteLater();
    qDebug() << "Client disconnected, remaining:" << m_clients.size();
}

void Server::onBroadcastRequested(ConnectionHandler *origin, const QJsonObject &message)
{
    broadcastExcept(origin, message);
}

void Server::onStatusBroadcast(int orderId, const QString &newStatus, ConnectionHandler *origin)
{
    QJsonObject notification;
    notification[QLatin1String("type")] = QLatin1String("order_status_update_notification");
    notification[QLatin1String("data")] = QJsonObject{
        {QStringLiteral("order_id"), orderId},
        {QStringLiteral("new_status"), newStatus}};
    broadcastExcept(origin, notification);
}

void Server::broadcastExcept(ConnectionHandler *origin, const QJsonObject &message)
{
    for (ConnectionHandler *client : std::as_const(m_clients)) {
        if (client != origin)
            client->sendJson(message);
    }
}
