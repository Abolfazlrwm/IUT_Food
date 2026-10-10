#ifndef PROFILEPANEL_H
#define PROFILEPANEL_H

#include <QWidget>
#include <QJsonObject>

class QListWidget;
class QStackedWidget;
class QLineEdit;
class QLabel;

class DataBaseHandler;

//! Customer profile: order history with one-click reorder, a wallet page
//! (balance, top-up, transaction log), order details with live chat,
//! delivery confirmation + review, and account settings.
class ProfilePanel : public QWidget
{
    Q_OBJECT
public:
    explicit ProfilePanel(DataBaseHandler *dbHandler, int userId,
                          const QString &username, QWidget *parent = nullptr);

public slots:
    void refreshHistory();
    void refreshWallet();

signals:
    void newChatMessage(int orderId, const QString &sender, const QString &message);
    //! Emitted once per dish when the user hits "reorder"; Client pushes
    //! them into the shared shopping cart.
    void reorderRequested(const QJsonObject &foodData, int quantity);

private:
    void setupUi();
    QWidget *createHistoryPage();
    QWidget *createWalletPage();
    QWidget *createSettingsPage();
    QWidget *createOrderDetailPage();
    void loadOrderDetails(int orderId);
    void markOrderDelivered();
    void reorderSelectedOrder();

    DataBaseHandler *m_dbHandler;
    int m_userId;
    QString m_username;

    QListWidget *m_historyList = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    QListWidget *m_navigationList = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_addressEdit = nullptr;
    QLabel *m_walletBalanceLabel = nullptr;
    QListWidget *m_walletTxList = nullptr;

    int m_currentOrderId = -1;
    int m_currentRestaurantId = -1;
    int m_currentRating = 0;
};

#endif // PROFILEPANEL_H
