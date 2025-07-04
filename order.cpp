#include "order.h"

Order::Order(int id, const QString& customerName, const QList<FoodItem>& items, const QString& status)
    : id(id), customerName(customerName), items(items), status(status) {}
Order::Order() : id(-1), customerName(""), status("") {}

int Order::getId() const { return id; }
QString Order::getCustomerName() const { return customerName; }
QList<FoodItem> Order::getItems() const { return items; }
QString Order::getStatus() const { return status; }

double Order::getTotalPrice() const {
    double total = 0.0;
    for (const auto& item : items) {
        total += item.price;  // چون FoodItem ساده‌ست، متد لازم نیست
    }
    return total;
}

void Order::setStatus(const QString& newStatus) {
    status = newStatus;
}
