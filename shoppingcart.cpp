#include "shoppingcart.h"
#include <QDebug>

ShoppingCart* ShoppingCart::instance = nullptr;

ShoppingCart* ShoppingCart::getInstance() {
    if (!instance)
        instance = new ShoppingCart();
    return instance;
}

ShoppingCart::ShoppingCart(QObject *parent) : QObject(parent) {}

void ShoppingCart::addItem(const QJsonObject &foodData, int quantity)
{
    int foodId = foodData["id"].toInt();

    // اگر این غذا از قبل در سبد بود، فقط تعدادش را اضافه کن
    if (m_items.contains(foodId)) {
        m_items[foodId].quantity += quantity;
    } else {
        // اگر نبود، یک آیتم جدید بساز
        CartItem newItem;
        newItem.foodData = foodData;
        newItem.quantity = quantity;
        m_items.insert(foodId, newItem);
    }
    qDebug() << "Item added/updated:" << foodData["name"].toString() << "Quantity:" << m_items[foodId].quantity;
    emit cartUpdated(); // به همه اطلاع بده که سبد خرید آپدیت شد
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
