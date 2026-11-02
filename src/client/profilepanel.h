#ifndef PROFILEPANEL_H
#define PROFILEPANEL_H

#include <QWidget>
#include <QJsonObject>

class QListWidget;
class QStackedWidget;
class QLineEdit;

class DataBaseHandler;

//! Customer profile: order history, order details with live chat,
//! delivery confirmation + review, and account settings.
class ProfilePanel : public QWidget
{
    Q_OBJECT
public:
    explicit ProfilePanel(DataBaseHandler *dbHandler, int userId,
                          const QString &username, QWidget *parent = nullptr);

public slots:
    void refreshHistory();

signals:
    void newChatMessage(int orderId, const QString &sender, const QString &message);

private:
    void setupUi();
    QWidget *createHistoryPage();
    QWidget *createSettingsPage();
    QWidget *createOrderDetailPage();
    void loadOrderDetails(int orderId);
    void markOrderDelivered();

    DataBaseHandler *m_dbHandler;
    int m_userId;
    QString m_username;

    QListWidget *m_historyList = nullptr;
    QStackedWidget *m_contentStack = nullptr;
    QListWidget *m_navigationList = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_addressEdit = nullptr;

    int m_currentOrderId = -1;
    int m_currentRestaurantId = -1;
    int m_currentRating = 0;
};

#endif // PROFILEPANEL_H
