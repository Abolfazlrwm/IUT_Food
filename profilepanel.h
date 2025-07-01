#ifndef PROFILEPANEL_H
#define PROFILEPANEL_H

#include <QWidget>
#include <QListWidgetItem> // برای استفاده در اسلات

// Forward declarations
class DataBaseHandler;
class QStackedWidget;
class QListWidget;

class ProfilePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePanel(DataBaseHandler *dbHandler, QWidget *parent = nullptr);
    ~ProfilePanel();

public slots:
    void refreshHistory();
    void displayNewMessage(int orderId, const QString& sender, const QString& message);

private slots:
    void onHistoryItemClicked(QListWidgetItem *item);

private:
    void setupUI();
    QWidget* createSettingsPage();
    QWidget* createHistoryPage();
    QWidget* createOrderDetailPage();
    void loadOrderDetails(int orderId);

    // اعضای کلاس
    QStackedWidget* m_contentStack;
    QListWidget* m_navigationList;
    DataBaseHandler* m_dbHandler;
    int m_currentOrderId;
};

#endif // PROFILEPANEL_H
