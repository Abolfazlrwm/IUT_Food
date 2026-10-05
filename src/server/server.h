#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QJsonObject>
#include <QList>
#include <QSslCertificate>
#include <QSslKey>

#include "sessionmanager.h"

class ConnectionHandler;
class DataBaseHandler;
class QTcpSocket;

//! Process-wide operational counters, rendered by the admin-only
//! `get_server_stats` command (and handy for the benchmark log).
struct ServerStats
{
    qint64 startedAtMs = 0;              //!< QDateTime::currentMSecsSinceEpoch() at startup
    std::atomic<quint64> connectionsAccepted{0};
    std::atomic<quint64> tlsConnections{0};   //!< connections that completed a TLS handshake
    std::atomic<quint64> requestsServed{0};   //!< commands that reached the dispatcher
    std::atomic<quint64> requestsRejected{0}; //!< rate-limit / frame-cap rejections
    std::atomic<quint64> authFailures{0};     //!< missing, expired or low-role tokens
};

//! Listens on TCP and spawns one ConnectionHandler per client.  Chat
//! messages and order-status changes are broadcast to every other
//! connected client, which is what makes multi-window demos work.
//! Since v2.2 the server also owns the SessionManager: every command
//! except `login` must present a token minted by it.
//!
//! Since v2.3 the listener can be switched into TLS mode with
//! enableTls(): every incoming connection then performs a standard
//! TLS handshake (QSslSocket::startServerEncryption) before the
//! ConnectionHandler is attached, so the JSON lines of the protocol
//! never travel in clear text.  The framing is byte-identical in both
//! modes; see docs/PROTOCOL.md §12.
class Server : public QTcpServer
{
    Q_OBJECT
public:
    explicit Server(DataBaseHandler *dbHandler, QObject *parent = nullptr);

    SessionManager &sessions() { return m_sessions; }
    ServerStats &stats() { return m_stats; }

    //! Loads the PEM certificate chain + private key used for new
    //! connections.  Call before listen(); returns false (and keeps the
    //! server in plain mode) when either file cannot be parsed.
    bool enableTls(const QString &certPath, const QString &keyPath);
    bool isTlsEnabled() const { return m_tlsEnabled; }

    //! v2.4: graceful shutdown - answers every connected client with a
    //! final "server_shutdown" notification and closes the sockets, so
    //! `docker stop` / Ctrl+C end the process cleanly instead of
    //! dropping connections on the floor.
    void disconnectClients();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected(ConnectionHandler *handler);
    void onBroadcastRequested(ConnectionHandler *origin, const QJsonObject &message);
    void onStatusBroadcast(int orderId, const QString &newStatus, ConnectionHandler *origin);

private:
    //! Wraps an accepted socket (plain or already encrypted) into a
    //! ConnectionHandler and registers it for broadcasts.
    void spawnHandler(QTcpSocket *socket);
    void broadcastExcept(ConnectionHandler *origin, const QJsonObject &message);

    QList<ConnectionHandler *> m_clients;
    DataBaseHandler *m_dbHandler;
    SessionManager m_sessions;
    ServerStats m_stats;

    // TLS mode (v2.3): empty chain = plain TCP.
    bool m_tlsEnabled = false;
    QList<QSslCertificate> m_tlsChain;
    QSslKey m_tlsKey;
};

#endif // SERVER_H
