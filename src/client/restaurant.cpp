#include "restaurant.h"
#include "databasehandler.h"

#include <QDebug>

Restaurant::Restaurant(int id, const QString &username, QObject *parent)
    : User(id, username, QString(), QString(), QStringLiteral("restaurant"), parent),
      m_pollTimer(new QTimer(this))
{
    // QTimer is a child object; it must not be deleted manually here
    // (the old destructor did exactly that).
    connect(m_pollTimer, &QTimer::timeout, this, &Restaurant::checkForNewData);
    m_pollTimer->start(5000);

    loadProfileFromDB();
    loadMenuFromDB();
    loadOrdersFromDB();
}

Restaurant::~Restaurant()
{
    m_pollTimer->stop();
}

bool Restaurant::saveProfileToDB()
{
    return DataBaseHandler::instance().updateRestaurantDetails(
        id, getName(), m_type, m_location, m_priceRange);
}

bool Restaurant::addFood(const QString &name, double price, const QString &description)
{
    if (!DataBaseHandler::instance().isOpen()) {
        qDebug() << "Database connection is not open!";
        return false;
    }
    if (name.isEmpty() || price <= 0) {
        qDebug() << "Invalid food data";
        return false;
    }

    const bool success = DataBaseHandler::instance().addFoodItem(id, name, price, description);
    if (success) {
        loadMenuFromDB();
        emit menuChanged();
    }
    return success;
}

bool Restaurant::updateFood(int foodId, const QString &name, double price,
                            const QString &description)
{
    const bool success = DataBaseHandler::instance().updateFoodItem(foodId, name, price, description);
    if (success)
        loadMenuFromDB();
    return success;
}

bool Restaurant::removeFood(int foodId)
{
    const bool success = DataBaseHandler::instance().deleteFoodItem(foodId);
    if (success)
        loadMenuFromDB();
    return success;
}

FoodItem Restaurant::getFoodItemById(int foodId) const
{
    for (const FoodItem &item : m_menu) {
        if (item.id == foodId)
            return item;
    }
    return FoodItem{};
}

bool Restaurant::updateOrderStatus(int orderId, const QString &newStatus)
{
    const bool success = DataBaseHandler::instance().updateOrderStatus(orderId, newStatus);
    if (success)
        loadOrdersFromDB();
    return success;
}

void Restaurant::loadProfileFromDB()
{
    QSqlQuery query = DataBaseHandler::instance().readRestaurantDetails(id);
    if (query.next()) {
        setName(query.value(QStringLiteral("name")).toString());
        m_type = query.value(QStringLiteral("restaurant_type")).toString();
        m_location = query.value(QStringLiteral("location")).toString();
        m_priceRange = query.value(QStringLiteral("price_range")).toInt();
        emit profileChanged();
    }
}

void Restaurant::loadMenuFromDB()
{
    m_menu = DataBaseHandler::instance().getMenuForRestaurant(id);
    emit menuChanged();
}

void Restaurant::loadOrdersFromDB()
{
    m_orders.clear();
    QSqlQuery query = DataBaseHandler::instance().getOrdersForRestaurant(id);

    QString lastCustomerName;
    while (query.next()) {
        Order order;
        order.id = query.value(QStringLiteral("id")).toInt();
        order.customerUsername = query.value(QStringLiteral("username")).toString();
        order.status = query.value(QStringLiteral("status")).toString();
        order.totalPrice = query.value(QStringLiteral("total_price")).toDouble();
        order.createdAt = query.value(QStringLiteral("created_at")).toDateTime();
        order.restaurantId = id;
        m_orders.append(order);
        if (lastCustomerName.isEmpty())
            lastCustomerName = order.customerUsername;
    }

    if (m_orders.count() > m_lastOrderCount && !lastCustomerName.isEmpty())
        emit newOrderReceived(lastCustomerName);
    m_lastOrderCount = m_orders.count();
    emit ordersChanged();
}

void Restaurant::checkForNewData()
{
    const int currentCount = DataBaseHandler::instance().getOrderCountForRestaurant(id);
    if (currentCount > m_lastOrderCount)
        loadOrdersFromDB();
}
