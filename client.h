#ifndef CLIENT_H
#define CLIENT_H
#include <QSqlQuery>
#include <QMainWindow>

// Forward declarations
class ProfilePanel;
class ShoppingCartPopup;
class QMenu;
class DataBaseHandler;
QT_BEGIN_NAMESPACE
namespace Ui { class Client; }
QT_END_NAMESPACE

class Client : public QMainWindow
{
    Q_OBJECT

public:
    Client(DataBaseHandler* dbHandler, QWidget *parent = nullptr);
    ~Client();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_actionProfile_triggered();
    void on_applyFilterButton_clicked();
    // اسلات‌های سبد خرید
    void onCartMenuAboutToShow();
    void onShowCheckoutDialog(); // <<< این اسلات فراموش شده بود

private:
    void setupToolbarActions();
    void createCartMenu(); // این تابع را از setupToolbarActions جدا می‌کنیم برای تمیزی بیشتر
    void populateRestaurantList(QSqlQuery &query);
    Ui::Client *ui;

    ProfilePanel *m_profilePanel;
    ShoppingCartPopup *m_cartPopup;
    QMenu *m_cartMenu;
    DataBaseHandler *m_dbHandler;
};
#endif // CLIENT_H
