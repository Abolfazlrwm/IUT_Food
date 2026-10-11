#ifndef RESTAURANT_H
#define RESTAURANT_H

#include "user.h"
#include "fooditem.h"
#include "order.h"

#include <QList>
#include <QTimer>

//! Server-side session object for one signed-in restaurant: keeps the
//! profile, menu and incoming orders in memory and refreshes them from
//! the local database on a timer.
class Restaurant : public User
{
    Q_OBJECT
public:
    explicit Restaurant(int id, const QString &username, QObject *parent = nullptr);
    ~Restaurant() override;

    QString getType() const { return m_type; }
    QString getLocation() const { return m_location; }
    int getPriceRange() const { return m_priceRange; }
    void setType(const QString &type) { m_type = type; }
    void setLocation(const QString &location) { m_location = location; }
    void setPriceRange(int range) { m_priceRange = range; }
    bool saveProfileToDB();

    const QList<FoodItem> &getMenu() const { return m_menu; }
    bool addFood(const QString &name, double price, const QString &description);
    bool updateFood(int foodId, const QString &name, double price, const QString &description);
    bool removeFood(int foodId);
    FoodItem getFoodItemById(int foodId) const;

    const QList<Order> &getOrders() const { return m_orders; }
    bool updateOrderStatus(int orderId, const QString &newStatus);

signals:
    void profileChanged();
    void menuChanged();
    void ordersChanged();
    void newOrderReceived(const QString &customerName);

private:
    void loadProfileFromDB();
    void loadMenuFromDB();
    void loadOrdersFromDB();
    void checkForNewData();

    QString m_type;
    QString m_location;
    int m_priceRange = 0;
    QList<FoodItem> m_menu;
    QList<Order> m_orders;
    int m_lastOrderCount = 0;
    QTimer *m_pollTimer = nullptr;
};

#endif // RESTAURANT_H
