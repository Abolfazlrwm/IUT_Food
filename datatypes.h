// datatypes.h
#ifndef DATATYPES_H
#define DATATYPES_H

#include <QString>

struct Restaurant {
    int id;
    QString name;
    QString type;       // فیلتر بر اساس نوع غذا (فست فود، ایرانی و...)
    QString location;   // فیلتر بر اساس موقعیت
    int priceRange;     // فیلتر بر اساس قیمت (مثلا ۱=ارزان, ۲=متوسط, ۳=گران)
    QString logoPath;
};

#endif // DATATYPES_H
