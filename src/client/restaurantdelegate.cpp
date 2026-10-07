#include "restaurantdelegate.h"
#include "restaurantitemwidget.h"
#include "datatypes.h"

#include <QPainter>

RestaurantDelegate::RestaurantDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void RestaurantDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    if (!index.data().canConvert<RestaurantData>())
        return;

    RestaurantData restaurant = qvariant_cast<RestaurantData>(index.data());

    RestaurantItemWidget widget;
    widget.setRestaurantData(restaurant);
    widget.resize(option.rect.size());
    if (option.state & QStyle::State_Selected)
        widget.setStyleSheet(QStringLiteral("background-color: #cce5ff;"));

    painter->save();
    painter->translate(option.rect.topLeft());
    widget.render(painter);
    painter->restore();
}

QSize RestaurantDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(220, 64);
}
