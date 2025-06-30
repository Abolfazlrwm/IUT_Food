#ifndef DATATYPES_H
#define DATATYPES_H

#include <QString>
#include <QJsonObject>

struct Restaurant {
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
