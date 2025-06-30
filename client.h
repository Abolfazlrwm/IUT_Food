#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QJsonArray> // برای اسلات onRestaurantsReceived

// Forward declarations
class DataBaseHandler;
class ProfilePanel;
class ShoppingCartPopup;
class QMenu;
class QListWidgetItem; // برای اسلات onRestaurantClicked

QT_BEGIN_NAMESPACE
namespace Ui { class Client; }
QT_END_NAMESPACE

class Client : public QMainWindow
{
    Q_OBJECT
public:
    explicit Client(DataBaseHandler *dbHandler, QWidget *parent = nullptr);
    ~Client();
signals:
    void historyChanged();

    void newChatMessage(int orderId, const QString& sender, const QString &message);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private slots:
    void on_actionProfile_triggered();
    void onCartMenuAboutToShow();
    void onShowCheckoutDialog();
    void on_applyFilterButton_clicked();
    void onRestaurantClicked(QListWidgetItem *item);
    void onRestaurantsReceived(const QJsonArray& restaurantsData); // اسلات برای دریافت داده از شبکه

    void onOrderStatusUpdated(const QJsonObject& orderData);
    void onNewChatMessage(const QJsonObject& chatData); // اسلات جدید

private:
    void setupActions();
    void createCartMenu();
    void populateRestaurantList(); // این تابع دیگر ورودی ندارد
    Ui::Client *ui;
    DataBaseHandler *m_dbHandler;
    ProfilePanel *m_profilePanel;
    ShoppingCartPopup *m_cartPopup;
    QMenu *m_cartMenu;
    // MenuPage *m_menuPage; // این را بعدا اضافه می‌کنیم
};
#endif // CLIENT_H
