#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>

// Forward declarations
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
    Client(QWidget *parent = nullptr);
    ~Client();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_actionProfile_triggered();

    // اسلات‌های سبد خرید
    void onCartMenuAboutToShow();
    void onShowCheckoutDialog(); // <<< این اسلات فراموش شده بود

private:
    void setupToolbarActions();
    void createCartMenu(); // این تابع را از setupToolbarActions جدا می‌کنیم برای تمیزی بیشتر

    Ui::Client *ui;

    ProfilePanel *m_profilePanel;
    ShoppingCartPopup *m_cartPopup;
    QMenu *m_cartMenu;
};
#endif // CLIENT_H
