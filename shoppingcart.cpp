#include "shoppingcart.h"
#include <QDebug>
#include <QVariant>


ShoppingCart::ShoppingCart(QObject *parent) : QObject(parent) {}

ShoppingCart* ShoppingCart::instance = nullptr;

ShoppingCart* ShoppingCart::getInstance() {
    if (!instance) instance = new ShoppingCart();
    return instance;
}

void ShoppingCart::addItem(const QJsonObject &foodData, int quantity) {
    int foodId = foodData["id"].toInt();
    if (m_items.contains(foodId)) {
        m_items[foodId].quantity += quantity;
    } else {
        CartItem newItem;
        newItem.foodData = foodData;
        newItem.quantity = quantity;
        m_items.insert(foodId, newItem);
    }
    qDebug() << "Cart updated! Item:" << foodData["name"].toString();
    emit cartUpdated(); // اطلاع‌رسانی به همه
}

const QMap<int, CartItem>& ShoppingCart::getItems() const
{
    return m_items;
}

double ShoppingCart::getTotalPrice() const
{
    double total = 0;
    for (const CartItem &item : m_items) {
        total += item.foodData["price"].toDouble() * item.quantity;
    }
    return total;
}

void ShoppingCart::clearCart()
{
    m_items.clear();
    emit cartUpdated();
}
