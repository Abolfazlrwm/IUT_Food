#include "server.h"
#include "connectionhandler.h"
#include "../../IUT_Food/databasehandler.h"
#include <QDebug>

Server::Server(DataBaseHandler* dbHandler, QObject* parent)
    : QTcpServer(parent), m_dbHandler(dbHandler)
{
}

void Server::incomingConnection(qintptr socketDescriptor)
{
    qDebug() << "🔌 New incoming connection with descriptor:" << socketDescriptor;

    auto* handler = new ConnectionHandler(socketDescriptor, m_dbHandler, this);

    connect(handler, &ConnectionHandler::disconnected,
            this, &Server::onClientDisconnected);

    connect(handler, &ConnectionHandler::newMessageBroadcast,
            this, &Server::broadcastMessage);

    m_clients.append(handler);

    qDebug() << "✅ Client connected. Total clients:" << m_clients.size();
}

void Server::onClientDisconnected(ConnectionHandler* handler)
{
    m_clients.removeAll(handler);
    handler->deleteLater();
    qDebug() << "❌ Client disconnected. Remaining clients:" << m_clients.size();
}

void Server::broadcastMessage(int orderId, const QJsonObject& message)
{
    for (ConnectionHandler* client : m_clients) {
        if (client->hasOrder(orderId)) {
            client->sendJson(message);
        }
    }
}
