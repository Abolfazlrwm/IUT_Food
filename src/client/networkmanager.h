#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QQueue>

class QSslSocket;

/*!
 * Singleton TCP client shared by every window of the client application.
 *
 * The wire format is newline-delimited JSON, exactly what the server
 * expects (ConnectionHandler reads with canReadLine).  The original
 * codebase had two parallel classes - this one and AdminNetworkManager -
 * and this one forgot the trailing '\n', so the server never answered
 * any customer request.  They are now a single, correct implementation.
 *
 * Since server v2.2 the wire protocol is authenticated: the manager
 * sends a `login` request once (authenticateWith) and the server
 * answers with a session token.  Every subsequent request automatically
 * carries that token, and requests issued before the session is
 * established are queued (up to a small cap) and flushed afterwards -
 * that keeps panels which fire requests in their constructors working
 * without any special-casing.
 *
 * Since v2.3 the connection can be wrapped in TLS: pass useTls to
 * connectToServer() and the socket becomes a QSslSocket speaking to a
 * server started with --tls-cert/--tls-key.  Self-signed demo
 * certificates are accepted when acceptSelfSigned is set (the login
 * dialog exposes both switches and remembers them via QSettings).
 */
class NetworkManager : public QObject
{
    Q_OBJECT
public:
    static NetworkManager &instance();

    //! Remembers the connection settings and (re)connects.  Safe to call
    //! while a connection attempt is already running.
    void connectToServer(const QString &host = QStringLiteral("127.0.0.1"),
                         quint16 port = 1234,
                         bool useTls = false,
                         bool acceptSelfSigned = true);
    void disconnectFromServer();
    bool isConnected() const;
    //! True when the current socket is an encrypted QSslSocket.
    bool isUsingTls() const;

    //! Appends the '\n' framing and sends; silently drops when offline.
    void sendJson(const QJsonObject &jsonObj);

    //! Requests the server session for \p username.  While the login
    //! round-trip is in flight, outgoing requests wait in a queue.
    void authenticateWith(const QString &username, const QString &password);

    //! True once a session token has been received for this connection.
    bool hasSession() const { return !m_sessionToken.isEmpty(); }

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &message);

    //! Emitted when the server accepts (or rejects) the session login.
    void sessionEstablished(bool success, const QString &role, int userId);

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

    //! Connects using the settings stored by connectToServer() - also
    //! used for the automatic re-login path of authenticateWith().
    void connectNow();
    //! (Re)builds the socket object so its type matches m_useTls and
    //! wires every signal.  The login request itself is queued by
    //! QSslSocket until the handshake has finished.
    void ensureSocketType();
    void dispatch(const QJsonObject &jsonObj);
    void flushQueue();

    QTcpSocket *m_socket = nullptr;
    bool m_useTls = false;
    bool m_acceptSelfSigned = true;
    QString m_host = QStringLiteral("127.0.0.1");
    quint16 m_port = 1234;
    QString m_sessionToken;
    QQueue<QJsonObject> m_pendingBeforeSession;
    //! Credentials waiting for the TCP handshake to finish; wiped once
    //! the login request has been put on the wire.
    QString m_pendingUser;
    QString m_pendingPassword;
    static constexpr int MaxPendingBeforeSession = 64;
};

#endif // NETWORKMANAGER_H
