#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QJsonObject>
#include <QList>

class ConnectionHandler;
class DataBaseHandler;

//! Listens on TCP and spawns one ConnectionHandler per client.  Chat
//! messages and order-status changes are broadcast to every other
//! connected client, which is what makes multi-window demos work.
class Server : public QTcpServer
{
    Q_OBJECT
public:
    explicit Server(DataBaseHandler *dbHandler, QObject *parent = nullptr);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected(ConnectionHandler *handler);
    void onBroadcastRequested(ConnectionHandler *origin, const QJsonObject &message);
    void onStatusBroadcast(int orderId, const QString &newStatus, ConnectionHandler *origin);

private:
    void broadcastExcept(ConnectionHandler *origin, const QJsonObject &message);

    QList<ConnectionHandler *> m_clients;
    DataBaseHandler *m_dbHandler;
};

#endif // SERVER_H
