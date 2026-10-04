#ifndef ORDER_H
#define ORDER_H

#include <QString>
#include <QDateTime>
#include <QList>
#include "fooditem.h"

//! In-memory snapshot of one order as shown in the restaurant dashboard.
struct Order
{
    int id = -1;
    int customerId = -1;
    QString customerUsername;
    int restaurantId = -1;
    QString status;
    double totalPrice = 0.0;
    QDateTime createdAt;
    QList<FoodItem> items;
};

#endif // ORDER_H
