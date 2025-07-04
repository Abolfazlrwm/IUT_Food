    #ifndef FOODITEM_H
#define FOODITEM_H

#include <QString>

// این کلاس به یک struct ساده تبدیل شده چون فقط برای نگهداری داده است.
// این کار نیاز به فایل fooditem.cpp را از بین می‌برد.
struct FoodItem {
    int id;
    int restaurantId;
    QString name;
    double price;
    QString description;
};

#endif // FOODITEM_H
