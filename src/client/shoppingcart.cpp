#include "shoppingcart.h"

#include <QDebug>

ShoppingCart *ShoppingCart::instance = nullptr;

ShoppingCart *ShoppingCart::getInstance()
{
    if (!instance)
        instance = new ShoppingCart();
    return instance;
}

ShoppingCart::ShoppingCart(QObject *parent) : QObject(parent) {}

void ShoppingCart::addItem(const QJsonObject &foodData, int quantity)
{
    const int foodId = foodData.value(QLatin1String("id")).toInt();
    if (m_items.contains(foodId)) {
        m_items[foodId].quantity += quantity;
    } else {
        CartItem newItem;
        newItem.foodData = foodData;
        newItem.quantity = quantity;
        m_items.insert(foodId, newItem);
    }
    emit cartUpdated();
}

const QMap<int, CartItem> &ShoppingCart::getItems() const
{
    return m_items;
}

double ShoppingCart::getTotalPrice() const
{
    double total = 0;
    for (const CartItem &item : m_items)
        total += item.foodData.value(QLatin1String("price")).toDouble() * item.quantity;
    return total;
}

void ShoppingCart::clearCart()
{
    m_items.clear();
    emit cartUpdated();
}
