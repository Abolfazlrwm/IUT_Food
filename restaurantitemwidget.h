#ifndef RESTAURANTITEMWIDGET_H
#define RESTAURANTITEMWIDGET_H

#include <QWidget>
#include "datatypes.h" // لازم است تا struct Restaurant را بشناسد

namespace Ui { class RestaurantItemWidget; }

class RestaurantItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RestaurantItemWidget(QWidget *parent = nullptr);
    ~RestaurantItemWidget();

    // این تابع، اطلاعات یک رستوران را می‌گیرد و در UI نمایش می‌دهد
    void setRestaurantData(const Restaurant &restaurant);

private:
    // این اشاره‌گر، پل ارتباطی بین کد C++ ما و ویجت‌های داخل فایل .ui است
    Ui::RestaurantItemWidget *ui;
};

#endif // RESTAURANTITEMWIDGET_H
