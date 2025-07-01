#include "restaurantdelegate.h"
#include "restaurantitemwidget.h"
#include "datatypes.h"
#include <QPainter>

RestaurantDelegate::RestaurantDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void RestaurantDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.data().canConvert<Restaurant>()) {
        Restaurant restaurant = qvariant_cast<Restaurant>(index.data());

        RestaurantItemWidget widget;
        widget.setRestaurantData(restaurant);
        widget.resize(option.rect.size());

        painter->save();
        painter->translate(option.rect.topLeft());
        widget.render(painter);
        painter->restore();
    }
}

QSize RestaurantDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    // یک اندازه ثابت برای هر آیتم
    return QSize(200, 70);
}
