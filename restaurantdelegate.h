#ifndef RESTAURANTDELEGATE_H
#define RESTAURANTDELEGATE_H

#include <QStyledItemDelegate>

class RestaurantDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit RestaurantDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // RESTAURANTDELEGATE_H
