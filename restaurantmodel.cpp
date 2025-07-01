#include "restaurantmodel.h"
#include <QVariant>
#include <QDebug>

RestaurantModel::RestaurantModel(QObject *parent) : QAbstractListModel(parent) {}

int RestaurantModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_restaurants.count();
}

QVariant RestaurantModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant();
    }
    return QVariant::fromValue(m_restaurants.at(index.row()));
}

void RestaurantModel::populateData(QSqlQuery &query) {
    beginResetModel();
    m_restaurants.clear();
    while (query.next()) {
        Restaurant r;
        r.id = query.value("id").toInt();
        r.name = QString::fromUtf8(query.value("name").toByteArray());
        r.type = QString::fromUtf8(query.value("type").toByteArray());
        r.location = QString::fromUtf8(query.value("location").toByteArray());
        m_restaurants.append(r);
    }
    endResetModel();
    qDebug() << m_restaurants.count() << "restaurants loaded into model.";
}

Restaurant RestaurantModel::getRestaurant(int row) const {
    if(row >= 0 && row < m_restaurants.count())
        return m_restaurants.at(row);
    return Restaurant();
}
