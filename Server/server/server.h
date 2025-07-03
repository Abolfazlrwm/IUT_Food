#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QList>
#include "connectionhandler.h"

class DataBaseHandler;

class Server : public QTcpServer
{
    Q_OBJECT

public:
    explicit Server(DataBaseHandler* dbHandler, QObject* parent = nullptr);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected(ConnectionHandler* handler);
    void broadcastMessage(int orderId, const QJsonObject& message);

private:
    QList<ConnectionHandler*> m_clients;
    DataBaseHandler* m_dbHandler;
};

#endif // SERVER_H
