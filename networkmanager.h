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
    void requestMenu(int restaurantId);
    void requestOrders(int restaurantId);
    void addFoodItem(int restaurantId, const QString& name, double price, const QString& description);
    void editFoodItem(int foodId, const QString& name, double price, const QString& description);
    void deleteFoodItem(int foodId);
    void updateOrderStatus(int orderId, const QString& newStatus);

signals:
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError socketError);
    // سیگنال‌هایی برای پاسخ‌های مختلف از سرور
    void loginResponse(bool success, const QString& message);
    void restaurantsReceived(const QJsonArray& restaurantsData);
    void orderStatusUpdated(const QJsonObject& orderData);
    void newMessageReceived(const QJsonObject& messageData);
    void menuReceived(const QJsonArray& menuData);
    void ordersReceived(const QJsonArray& ordersData);
    void operationResult(bool success, const QString& message);

private slots:
    void onReadyRead();

private:
    explicit NetworkManager(QObject *parent = nullptr);
    static NetworkManager* instance;
    QTcpSocket *m_socket;
    NetworkManager* m_network;

};

#endif // NETWORKMANAGER_H
