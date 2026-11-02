#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>

/*!
 * Singleton TCP client shared by every window of the client application.
 *
 * The wire format is newline-delimited JSON, exactly what the server
 * expects (ConnectionHandler reads with canReadLine).  The original
 * codebase had two parallel classes - this one and AdminNetworkManager -
 * and this one forgot the trailing '\n', so the server never answered
 * any customer request.  They are now a single, correct implementation.
 */
class NetworkManager : public QObject
{
    Q_OBJECT
public:
    static NetworkManager &instance();

    void connectToServer(const QString &host = QStringLiteral("127.0.0.1"),
                         quint16 port = 1234);
    void disconnectFromServer();
    bool isConnected() const;

    //! Appends the '\n' framing and sends; silently drops when offline.
    void sendJson(const QJsonObject &jsonObj);

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &message);

    //! Every parsed server message, for generic consumers (admin panel).
    void jsonReceived(const QJsonObject &obj);

    void restaurantsReceived(const QJsonArray &restaurantsData);
    void orderStatusUpdated(const QJsonObject &orderData);
    void chatMessageReceived(const QJsonObject &messageData);
    void operationResult(const QString &command, bool success, const QString &message);

private slots:
    void onReadyRead();

private:
    explicit NetworkManager(QObject *parent = nullptr);
    NetworkManager(const NetworkManager &) = delete;
    NetworkManager &operator=(const NetworkManager &) = delete;

    QTcpSocket *m_socket;
};

#endif // NETWORKMANAGER_H
