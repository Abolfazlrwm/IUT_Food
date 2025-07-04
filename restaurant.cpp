#include "restaurant.h"
#include "databasehandler.h"
#include <QTimer>
#include <QDebug>

Restaurant::Restaurant(int id, const QString& username, QObject* parent)
    : User(id, username, "", "", "restaurant", parent),
    m_type(""), m_location(""), m_priceRange(0),
    m_lastOrderCount(0), m_pollTimer(new QTimer(this))
{
    connect(m_pollTimer, &QTimer::timeout, this, &Restaurant::checkForNewData);
    m_pollTimer->start(10000); // هر 10 ثانیه

    loadProfileFromDB();
    loadMenuFromDB();
    loadOrdersFromDB();
}

Restaurant::~Restaurant()
{
    if (m_pollTimer) {
        m_pollTimer->stop();
        delete m_pollTimer;
    }
}

bool Restaurant::saveProfileToDB()
{
    return DataBaseHandler::instance().updateRestaurantDetails(
        this->id,
        this->getName(),
        m_type,
        m_location,
        m_priceRange
        );
}

bool Restaurant::addFood(const QString& name, double price, const QString& description)
{
    if (!DataBaseHandler::instance().isOpen()) {
        qDebug() << "Database connection is not open!";
        return false;
    }

    if (name.isEmpty() || price <= 0) {
        qDebug() << "Invalid food data";
        return false;
    }

    bool success = DataBaseHandler::instance().addFoodItem(this->id, name, price, description);
    if (success) {
        loadMenuFromDB();
        emit menuChanged();
        qDebug() << "Food added successfully for restaurant ID:" << this->id;
    } else {
        qDebug() << "Failed to add food for restaurant ID:" << this->id;
    }
    return success;
}
bool Restaurant::updateFood(int foodId, const QString& name, double price, const QString& description)
{
    bool success = DataBaseHandler::instance().updateFoodItem(foodId, name, price, description);
    if (success) {
        loadMenuFromDB();
    }
    return success;
}

bool Restaurant::removeFood(int foodId)
{
    bool success = DataBaseHandler::instance().deleteFoodItem(foodId);
    if (success) {
        loadMenuFromDB();
    }
    return success;
}

FoodItem Restaurant::getFoodItemById(int foodId) const
{
    for (const FoodItem& item : m_menu) {
        if (item.id == foodId) {
            return item;
        }
    }
    return FoodItem();
}

bool Restaurant::updateOrderStatus(int orderId, const QString& newStatus)
{
    bool success = DataBaseHandler::instance().updateOrderStatus(orderId, newStatus);
    if (success) {
        loadOrdersFromDB();
    }
    return success;
}

Order Restaurant::getOrderById(int orderId) const
{
    for (const Order& order : m_orders) {
        if (order.id == orderId) {
            return order;
        }
    }
    return Order();
}

void Restaurant::loadProfileFromDB()
{
    QSqlQuery query = DataBaseHandler::instance().readRestaurantDetails(this->id);
    if (query.next()) {
        setName(query.value("name").toString());
        m_type = query.value("restaurant_type").toString();
        m_location = query.value("location").toString();
        m_priceRange = query.value("price_range").toInt();
        emit profileChanged();
    }
}

void Restaurant::loadMenuFromDB()
{
    m_menu = DataBaseHandler::instance().getMenuForRestaurant(this->id);
    emit menuChanged();
}

void Restaurant::loadOrdersFromDB()
{
    m_orders.clear();
    QSqlQuery query = DataBaseHandler::instance().getOrdersForRestaurant(this->id);

    int currentOrderCount = 0;
    QString lastCustomerName;

    while (query.next()) {
        currentOrderCount++;
        Order order;
        order.id = query.value("id").toInt();
        order.customerUsername = query.value("username").toString();
        order.status = query.value("status").toString();
        order.totalPrice = query.value("total_price").toDouble();
        order.createdAt = query.value("created_at").toDateTime();
        m_orders.append(order);

        if (lastCustomerName.isEmpty()) {
            lastCustomerName = order.customerUsername;
        }
    }

    if (currentOrderCount > m_lastOrderCount) {
        emit newOrderReceived(lastCustomerName);
    }
    m_lastOrderCount = currentOrderCount;
    emit ordersChanged();
}

void Restaurant::checkForNewData()
{
    int currentCount = DataBaseHandler::instance().getOrderCountForRestaurant(this->id);
    if (currentCount > m_lastOrderCount) {
        loadOrdersFromDB();
    }
}
