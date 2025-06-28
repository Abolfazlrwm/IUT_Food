#ifndef SHOPPINGCARTPOPUP_H
#define SHOPPINGCARTPOPUP_H

#include <QWidget>

class QListWidget;
class QLabel;
class QPushButton;

class ShoppingCartPopup : public QWidget
{
    Q_OBJECT
public:
    explicit ShoppingCartPopup(QWidget *parent = nullptr);
    void updateContent();

signals:
    void checkoutRequested(); // سیگنال برای شروع فرآیند پرداخت

private:
    QListWidget* m_itemList;
    QLabel* m_totalPriceLabel;
    QPushButton* m_checkoutButton;
};

#endif // SHOPPINGCARTPOPUP_H
