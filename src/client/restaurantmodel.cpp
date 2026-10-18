#include "restaurantmodel.h"

#include <QSqlRecord>

RestaurantModel::RestaurantModel(QObject *parent) : QAbstractListModel(parent) {}

int RestaurantModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_restaurants.count();
}

QVariant RestaurantModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();
    return QVariant::fromValue(m_restaurants.at(index.row()));
}

namespace {
QString readString(const QSqlQuery &query, std::initializer_list<const char *> columns)
{
    for (const char *column : columns) {
        const int idx = query.record().indexOf(QString::fromLatin1(column));
        if (idx >= 0)
            return query.value(idx).toString();
    }
    return QString();
}
} // namespace

void RestaurantModel::populateData(QSqlQuery &query)
{
    beginResetModel();
    m_restaurants.clear();
    while (query.next()) {
        RestaurantData r;
        r.id = query.value(QStringLiteral("id")).toInt();
        r.name = readString(query, {"name"});
        r.type = readString(query, {"type", "restaurant_type"});
        r.location = readString(query, {"location"});
        r.priceRange = query.record().indexOf(QStringLiteral("price_range")) >= 0
                           ? query.value(QStringLiteral("price_range")).toInt() : 0;
        m_restaurants.append(r);
    }
    endResetModel();
}

void RestaurantModel::appendUnique(QSqlQuery &query)
{
    while (query.next()) {
        RestaurantData r;
        r.id = query.value(QStringLiteral("id")).toInt();
        r.name = readString(query, {"name"});
        r.type = readString(query, {"type", "restaurant_type"});
        r.location = readString(query, {"location"});

        const bool alreadyShown = std::any_of(m_restaurants.cbegin(), m_restaurants.cend(),
                                              [&r](const RestaurantData &existing) {
                                                  return existing.name == r.name &&
                                                         existing.location == r.location;
                                              });
        if (alreadyShown)
            continue;

        beginInsertRows(QModelIndex(), m_restaurants.count(), m_restaurants.count());
        m_restaurants.append(r);
        endInsertRows();
    }
}

RestaurantData RestaurantModel::getRestaurant(int row) const
{
    if (row >= 0 && row < m_restaurants.count())
        return m_restaurants.at(row);
    return RestaurantData{};
}
