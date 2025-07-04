#ifndef RESTAURANTMODEL_H
#define RESTAURANTMODEL_H

#include <QAbstractListModel>
#include "datatypes.h"
#include <QSqlQuery>

class RestaurantModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit RestaurantModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void populateData(QSqlQuery &query);
    RestaurantData getRestaurant(int row) const;

private:
    QList<RestaurantData> m_restaurants;
};

#endif // RESTAURANTMODEL_H
