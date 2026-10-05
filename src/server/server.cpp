#include "server.h"
#include "connectionhandler.h"
#include "logger.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QSslCipher>
#include <QSslSocket>
#include <QTimer>

Server::Server(DataBaseHandler *dbHandler, QObject *parent)
    : QTcpServer(parent), m_dbHandler(dbHandler)
{
    m_stats.startedAtMs = QDateTime::currentMSecsSinceEpoch();

    // Reap expired sessions every minute so abandoned logins cannot
    // accumulate even when no request ever touches them again.
    auto *sweeper = new QTimer(this);
    connect(sweeper, &QTimer::timeout, this, [this] {
        m_sessions.sweep(QDateTime::currentMSecsSinceEpoch());
    });
    sweeper->start(60 * 1000);
}

bool Server::enableTls(const QString &certPath, const QString &keyPath)
{
    const QList<QSslCertificate> chain = QSslCertificate::fromPath(certPath);
    if (chain.isEmpty() || chain.first().isNull()) {
        qCritical() << "TLS: could not parse a certificate from" << certPath;
        return false;
    }

    QFile keyFile(keyPath);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qCritical() << "TLS: cannot open the private key file:" << keyPath;
        return false;
    }
    // The PEM container type is not announced anywhere, so try the two
    // common ones; demo certificates are usually RSA, modern ones often EC.
    QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
    if (key.isNull()) {
        keyFile.reset();
        key = QSslKey(&keyFile, QSsl::Ec, QSsl::Pem);
    }
    if (key.isNull()) {
        qCritical() << "TLS: could not parse the private key from" << keyPath;
        return false;
    }

    m_tlsChain = chain;
    m_tlsKey = key;
    m_tlsEnabled = true;
    return true;
}

void Server::incomingConnection(qintptr socketDescriptor)
{
    m_stats.connectionsAccepted.fetch_add(1, std::memory_order_relaxed);

    if (m_tlsEnabled) {
        auto *socket = new QSslSocket(this);
        if (!socket->setSocketDescriptor(socketDescriptor)) {
            delete socket;
            return;
        }
        socket->setLocalCertificateChain(m_tlsChain);
        socket->setPrivateKey(m_tlsKey);
        // No client certificates are requested, so nothing on the server
        // side is worth aborting over - log whatever the peer objects to.
        connect(socket, &QSslSocket::sslErrors, this,
                [socket](const QList<QSslError> &errors) {
            for (const QSslError &error : errors)
                qWarning() << "TLS server-side error:" << error.errorString();
            socket->ignoreSslErrors();
        });
        // A peer that vanishes before the handshake finishes must not leak.
        connect(socket, &QSslSocket::disconnected,
                socket, &QSslSocket::deleteLater);
        connect(socket, &QSslSocket::encrypted, this, [this, socket] {
            m_stats.tlsConnections.fetch_add(1, std::memory_order_relaxed);
            LOG_INFO("tls", QString("handshake complete: %1 | %2")
                                .arg(socket->peerAddress().toString(),
                                     socket->sessionCipher().name()));
            spawnHandler(socket);
        });
        socket->startServerEncryption();
        return;
    }

    auto *socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        delete socket;
        return;
    }
    spawnHandler(socket);
}

void Server::spawnHandler(QTcpSocket *socket)
{
    auto *handler = new ConnectionHandler(socket, m_dbHandler,
                                          &m_sessions, &m_stats, this);
    connect(handler, &ConnectionHandler::disconnected,
            this, &Server::onClientDisconnected);
    connect(handler, &ConnectionHandler::broadcastRequested,
            this, &Server::onBroadcastRequested);
    connect(handler, &ConnectionHandler::orderStatusChanged,
            this, &Server::onStatusBroadcast);

    m_clients.append(handler);
    LOG_INFO("server", QString("client connected (%1) | total clients: %2")
                           .arg(qobject_cast<QSslSocket *>(socket)
                                    ? QStringLiteral("tls")
                                    : QStringLiteral("plain"))
                           .arg(m_clients.size()));
}

void Server::disconnectClients()
{
    QJsonObject goodbye;
    goodbye[QLatin1String("type")] = QLatin1String("server_shutdown");
    const int count = m_clients.size(); // closing may emit disconnected() inline
    for (ConnectionHandler *client : std::as_const(m_clients)) {
        client->sendJson(goodbye);
        client->closeConnection();
    }
    LOG_INFO("server", QString("said goodbye to %1 client(s)").arg(count));
}

void Server::onClientDisconnected(ConnectionHandler *handler)
{
    m_clients.removeAll(handler);
    handler->deleteLater();
    LOG_INFO("server", QString("client disconnected, remaining: %1").arg(m_clients.size()));
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
