#ifndef FOODITEM_H
#define FOODITEM_H

#include <QString>

//! Plain data holder for a single dish on a restaurant menu.
struct FoodItem
{
    int id = -1;
    int restaurantId = -1;
    QString name;
    double price = 0.0;
    QString description;
};

#endif // FOODITEM_H
