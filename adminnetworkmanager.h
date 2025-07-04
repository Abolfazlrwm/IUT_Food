#ifndef ADMINNETWORKMANAGER_H
#define ADMINNETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>

class AdminNetworkManager : public QObject
{
    Q_OBJECT
public:
    explicit AdminNetworkManager(QObject* parent = nullptr);
    void connectToServer(const QString& ip, quint16 port);
    void sendJson(const QJsonObject& obj);

signals:
    void serverResponse(const QJsonObject& obj);
    void connected();
    void disconnected();
    void errorOccurred(const QString& msg);

private slots:
    void onReadyRead();

private:
    QTcpSocket* m_socket;
};

#endif // ADMINNETWORKMANAGER_H
