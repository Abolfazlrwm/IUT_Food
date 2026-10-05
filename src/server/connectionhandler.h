#ifndef CONNECTIONHANDLER_H
#define CONNECTIONHANDLER_H

#include <QObject>
#include <QJsonObject>
#include <QTcpSocket>

#include "ratelimiter.h"
#include "sessionmanager.h"

class DataBaseHandler;
struct ServerStats;

//! One instance per connected client: parses newline-delimited JSON
//! requests and dispatches them to the shared DataBaseHandler.
//!
//! Since v2.2 every command except `login` must carry the session
//! token returned by a successful login; requests without a valid
//! token are answered with an "unauthorized" error, and commands that
//! do not match the session's role (or try to touch another user's
//! data) are answered with "forbidden".  See docs/PROTOCOL.md.
class ConnectionHandler : public QObject
{
    Q_OBJECT
public:
    //! Requests larger than this are dropped and the connection closed;
    //! the newline-delimited protocol never legitimately needs them.
    static constexpr int MaxMessageBytes = 64 * 1024;

    //! Takes ownership of an already-accepted socket.  For TLS mode the
    //! server hands over a QSslSocket whose handshake has completed
    //! (Server::spawnHandler), so every byte this class writes is
    //! encrypted transparently - the framing stays identical.
    explicit ConnectionHandler(QTcpSocket *socket, DataBaseHandler *dbHandler,
                               SessionManager *sessions, ServerStats *stats,
                               QObject *parent = nullptr);
    ~ConnectionHandler() override;

    void sendJson(const QJsonObject &response);

    //! v2.4: graceful-shutdown helper - flushes whatever is queued and
    //! starts a clean TCP disconnect (the goodbye bytes written just
    //! before still reach the peer).
    void closeConnection();

signals:
    void disconnected(ConnectionHandler *handler);
    //! Chat message (or any notification) that should reach other clients.
    void broadcastRequested(ConnectionHandler *origin, const QJsonObject &message);
    //! Emitted after update_order_status so other sessions stay in sync.
    void orderStatusChanged(int orderId, const QString &newStatus, ConnectionHandler *origin);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    //! Times the dispatch and writes a debug request line, then hands
    //! over to the real dispatcher (v2.4 request logging).
    void processCommand(const QJsonObject &request);
    void dispatchCommand(const QJsonObject &request);
    void rejectAbuse(const char *message);

    //! Resolves the request's "token" field to an authenticated
    //! session.  On failure it fills \p response with the proper
    //! unauthorized error, sends it and returns false.
    bool authenticate(const QJsonObject &request, QJsonObject &response,
                      SessionManager::Session &session);
    //! Adds a "forbidden" error to \p response and sends it - used when
    //! the session's role does not match the command.
    void forbid(QJsonObject &response, const char *message);
    //! True when \p orderId belongs to the session's restaurant.
    bool orderBelongsToRestaurant(int orderId, int restaurantId);

    QTcpSocket *m_socket;
    DataBaseHandler *m_dbHandler;
    SessionManager *m_sessions; //!< shared across all connections
    ServerStats *m_stats;       //!< shared across all connections
    RateLimiter m_limiter;      //!< per-connection request budget
};

#endif // CONNECTIONHANDLER_H
