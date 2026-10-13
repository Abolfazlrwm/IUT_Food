#ifndef DATATYPES_H
#define DATATYPES_H

#include <QString>
#include <QJsonObject>
#include <QMetaType>

//! Lightweight value type describing one restaurant row (client model/view).
struct RestaurantData
{
    int id = -1;
    QString name;
    QString type;
    QString location;
    int priceRange = 0;
};

//! One shopping-cart entry: the food JSON as served by the menu plus a count.
struct CartItem
{
    QJsonObject foodData;
    int quantity = 0;
};

Q_DECLARE_METATYPE(RestaurantData)

#endif // DATATYPES_H
