#ifndef DATATYPES_H
#define DATATYPES_H

#include <QString>
#include <QJsonObject>

struct RestaurantData {
    int id;
    QString name;
    QString type;
    QString location;
    int priceRange;
};

struct CartItem {
    QJsonObject foodData;
    int quantity;
};

#endif // DATATYPES_H
