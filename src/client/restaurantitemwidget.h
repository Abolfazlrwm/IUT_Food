#ifndef RESTAURANTITEMWIDGET_H
#define RESTAURANTITEMWIDGET_H

#include <QWidget>
#include "datatypes.h"

namespace Ui { class RestaurantItemWidget; }

//! The visual card for a single restaurant row inside the list view.
class RestaurantItemWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RestaurantItemWidget(QWidget *parent = nullptr);
    ~RestaurantItemWidget() override;

    void setRestaurantData(const RestaurantData &restaurant);

private:
    Ui::RestaurantItemWidget *ui;
    int m_restaurantId = -1;
};

#endif // RESTAURANTITEMWIDGET_H
