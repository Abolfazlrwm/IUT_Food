// order.h
#ifndef ORDER_H
#define ORDER_H

#include <QString>
#include <QDateTime>
#include <QList>
#include "fooditem.h"

struct Order {
    int id;
    int customerId;
    QString customerUsername;
    int restaurantId;
    QString status;
    double totalPrice;
    QDateTime createdAt;
    QList<FoodItem> items;
};

#endif // ORDER_H
