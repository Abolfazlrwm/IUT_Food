#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>

class NetworkManager : public QObject
{
    Q_OBJECT
public:
    static NetworkManager* getInstance();
    void connectToServer(const QString& ip = "127.0.0.1", quint16 port = 1234);
    void sendJson(const QJsonObject& jsonObj);

signals:
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError socketError);
    // سیگنال‌هایی برای پاسخ‌های مختلف از سرور
    void loginResponse(bool success, const QString& message);
    void restaurantsReceived(const QJsonArray& restaurantsData);
    void orderStatusUpdated(const QJsonObject& orderData);

private slots:
    void onReadyRead();

private:
    explicit NetworkManager(QObject *parent = nullptr);
    static NetworkManager* instance;
    QTcpSocket *m_socket;
};

#endif // NETWORKMANAGER_H
