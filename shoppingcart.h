#ifndef SHOPPINGCART_H
#define SHOPPINGCART_H

#include <QObject>
#include <QMap>
#include <QJsonObject>
#include "datatypes.h" // برای دسترسی به struct CartItem

class ShoppingCart : public QObject
{
    Q_OBJECT
public:
    static ShoppingCart* getInstance();
    void addItem(const QJsonObject& foodData, int quantity);
    const QMap<int, CartItem>& getItems() const;
    double getTotalPrice() const;
    void clearCart();
signals:
    void cartUpdated(); // هر بار که سبد خرید تغییر کند، این سیگنال منتشر می‌شود
private:
    explicit ShoppingCart(QObject *parent = nullptr);
    static ShoppingCart* instance;
    QMap<int, CartItem> m_items;
};
#endif // SHOPPINGCART_H
