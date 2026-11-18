#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QJsonObject>

class DataBaseHandler;
class ProfilePanel;
class ShoppingCartPopup;
class OrderTrackerDialog;
class QMenu;
class QAction;
class QLabel;

namespace Ui { class Client; }

//! Signed-in customer workspace: browse restaurants, order food and
//! follow each order's status and chat.
class Client : public QMainWindow
{
    Q_OBJECT
public:
    explicit Client(int userId, const QString &username,
                    DataBaseHandler *dbHandler, QWidget *parent = nullptr);
    ~Client() override;

    int userId() const { return m_userId; }
    QString username() const { return m_username; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_showRestaurantsButton_clicked();
    void on_actionProfile_triggered();
    void onCartMenuAboutToShow();
    void onShowCheckoutDialog();

    void onNetworkStatusChanged();
    void onServerVersionReceived(const QString &version);
    void onOutdatedClient(const QString &serverVersion);
    void onOrderStatusUpdated(const QJsonObject &orderData);
    void onNewChatMessage(const QJsonObject &chatData);

signals:
    void historyChanged();
    void newChatMessage(int orderId, const QString &sender, const QString &message);

private:
    void setupActions();
    void createCartMenu();

    Ui::Client *ui;
    DataBaseHandler *m_dbHandler;
    int m_userId;
    QString m_username;

    //! v2.8: permanent status-bar widgets - the link state and the
    //! server release stay visible instead of flashing for 4 seconds.
    QLabel *m_connLabel = nullptr;
    QLabel *m_serverLabel = nullptr;

    ProfilePanel *m_profilePanel = nullptr;
    ShoppingCartPopup *m_cartPopup = nullptr;
    QMenu *m_cartMenu = nullptr;
    OrderTrackerDialog *m_trackerDialog = nullptr;
    QAction *m_themeAction = nullptr;
};

#endif // CLIENT_H
