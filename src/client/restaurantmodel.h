#ifndef RESTAURANTMODEL_H
#define RESTAURANTMODEL_H

#include <QAbstractListModel>
#include "datatypes.h"

#include <QSqlQuery>

//! List model over RestaurantData rows for the search-results view.
class RestaurantModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit RestaurantModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void populateData(QSqlQuery &query);
    void appendUnique(QSqlQuery &query); //!< adds rows not already present
    RestaurantData getRestaurant(int row) const;

private:
    QList<RestaurantData> m_restaurants;
};

#endif // RESTAURANTMODEL_H
