#ifndef CONNECTIONHANDLER_H
#define CONNECTIONHANDLER_H

#include <QObject>
#include <QJsonObject>
#include <QTcpSocket>

#include "ratelimiter.h"

class DataBaseHandler;

//! One instance per connected client: parses newline-delimited JSON
//! requests and dispatches them to the shared DataBaseHandler.
class ConnectionHandler : public QObject
{
    Q_OBJECT
public:
    //! Requests larger than this are dropped and the connection closed;
    //! the newline-delimited protocol never legitimately needs them.
    static constexpr int MaxMessageBytes = 64 * 1024;

    explicit ConnectionHandler(qintptr socketDescriptor, DataBaseHandler *dbHandler,
                               QObject *parent = nullptr);
    ~ConnectionHandler() override;

    void sendJson(const QJsonObject &response);

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
    void processCommand(const QJsonObject &request);
    void rejectAbuse(const char *message);

    QTcpSocket *m_socket;
    DataBaseHandler *m_dbHandler;
    RateLimiter m_limiter; //!< per-connection request budget
};

#endif // CONNECTIONHANDLER_H
