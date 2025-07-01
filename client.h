#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <QJsonObject>
#include <QJsonArray>

// Forward declarations
class DataBaseHandler;
class ProfilePanel;
class ShoppingCartPopup;
class QMenu;

QT_BEGIN_NAMESPACE
namespace Ui { class Client; }
QT_END_NAMESPACE

class Client : public QMainWindow
{
    Q_OBJECT

public:
    explicit Client(DataBaseHandler *dbHandler, QWidget *parent = nullptr);
    ~Client();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_showRestaurantsButton_clicked(); // اسلات برای دکمه اصلی

    // اسلات‌های مربوط به پروفایل و سبد خرید
    void on_actionProfile_triggered();
    void onCartMenuAboutToShow();
    void onShowCheckoutDialog();

    // اسلات‌های شبکه
    //void onRestaurantsReceived(const QJsonArray& restaurantsData);
    void onOrderStatusUpdated(const QJsonObject& orderData);
    void onNewChatMessage(const QJsonObject& chatData);

signals:
    void historyChanged();
    void newChatMessage(int orderId, const QString& sender, const QString& message);

private:
    void setupActions();
    void createCartMenu();

    Ui::Client *ui;
    DataBaseHandler *m_dbHandler;

    // اعضای مربوط به پنجره‌های جداگانه
    ProfilePanel *m_profilePanel;
    ShoppingCartPopup *m_cartPopup;
    QMenu *m_cartMenu;
};
#endif // CLIENT_H
