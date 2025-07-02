#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVector>
#include "fooditem.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>

class DataBaseHandler
{
public:
    bool createTestOrder(int restaurantId, int customerId, double totalPrice, const QString& status);
    static DataBaseHandler& instance();
    QSqlDatabase& getDatabase() { return m_db; }
    void logDatabaseStatus();

    bool initializeDatabase();
    bool isOpen() const;
    bool openDataBase(const QString& fileName);
    bool createTables();
    bool validateRestaurant(int restaurantId);
    int getOrderCountForRestaurant(int restaurantId);
    int getUserId(const QString& username);
    bool registerRestaurant(const QString& username, const QString& password,
                            const QString& name, const QString& type,
                            const QString& location, int priceRange);
    bool loginUser(const QString& userName, const QString& password,
                   QString& role, int& userId);
    QSqlQuery readUser(const QString& username);
    bool approveRestaurant(int userId);
    QSqlQuery readRestaurantDetails(int restaurantId);
    bool updateRestaurantDetails(int restaurantId, const QString& name,
                                 const QString& type, const QString& location,
                                 int priceRange);
    void checkDatabaseStatus();
    bool addFoodItem(int restaurantId, const QString& name,
                     double price, const QString& description);
    bool updateFoodItem(int foodId, const QString& name,
                        double price, const QString& description);
    bool deleteFoodItem(int foodId);
    QVector<FoodItem> getMenuForRestaurant(int restaurantId);
    QSqlQuery getFoodItem(int foodId);
    QSqlQuery getOrdersForRestaurant(int restaurantId);
    bool updateOrderStatus(int orderId, const QString& status);

private:
    DataBaseHandler();
    QSqlDatabase m_db;
};

#endif // DATABASEHANDLER_H
