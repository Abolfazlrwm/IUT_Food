#ifndef CONNECTIONHANDLER_H
#define CONNECTIONHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include "../../IUT_Food/databasehandler.h"

class ConnectionHandler : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionHandler(qintptr socketDescriptor, DataBaseHandler* dbHandler, QObject *parent = nullptr);
    ~ConnectionHandler();
    void sendJson(const QJsonObject& response);
    bool hasOrder(int orderId) const;

signals:
    void disconnected(ConnectionHandler* handler);
    void newMessageBroadcast(int orderId, const QJsonObject& message);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void processCommand(const QJsonObject& request);

    QTcpSocket* m_socket;
    DataBaseHandler* m_dbHandler;
    qintptr m_socketDescriptor;
    int m_currentOrderId = -1;

};

#endif // CONNECTIONHANDLER_H
