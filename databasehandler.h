#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVector>
#include <QJsonObject>
#include <QMap>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include "datatypes.h" // For CartItem struct
#include "fooditem.h"

class DataBaseHandler
{
public:
    // Singleton access (for thread safety and centralized DB management)
    static DataBaseHandler& instance();

    // Core database connection and setup
    QSqlDatabase& getDatabase() { return m_db; }
    bool openDataBase(const QString& fileName);
    bool createTables();
    bool initializeDatabase();
    void logDatabaseStatus();
    void checkDatabaseStatus();
    bool isOpen() const;

    // --- User & Admin Management ---
    bool registerUser(const QString& userName, const QString& password, const QString& role);
    QSqlQuery readUser(const QString& userName);
    QSqlQuery readAllUsers();
    bool deleteUser(const QString& userName);
    bool deleteUser(int userId);
    bool blockUser(int userId);
    bool unblockUser(int userId);
    bool approveRestaurant(int userId);
    bool disapproveRestaurant(int userId);
    bool loginUser(const QString& userName, const QString& password, QString& role, int& userId);
    // Overload for compatibility
    bool loginUser(const QString& userName, const QString& password); // legacy
    QString getUserRole(const QString& username);
    QSqlQuery getUserDetails(int userId);
    bool updateUserDetails(int userId, const QString& newUsername, const QString& newAddress);
    int getUserId(const QString& username);

    // --- Restaurant Management ---
    bool registerRestaurant(const QString& username, const QString& password,
                            const QString& name, const QString& type,
                            const QString& location, int priceRange);
    QSqlQuery readRestaurantDetails(int restaurantId);
    bool updateRestaurantDetails(int restaurantId, const QString& name,
                                 const QString& type, const QString& location,
                                 int priceRange);
    bool validateRestaurant(int restaurantId);
    int getOrderCountForRestaurant(int restaurantId);
    QSqlQuery getAllRestaurants(const QString& typeFilter, const QString& locationFilter, const QString& nameFilter);

    // --- Menu/Food Items ---
    bool addFoodItem(int restaurantId, const QString& name,
                     double price, const QString& description);
    bool updateFoodItem(int foodId, const QString& name,
                        double price, const QString& description);
    bool deleteFoodItem(int foodId);
    QVector<FoodItem> getMenuForRestaurant(int restaurantId);
    QSqlQuery getMenuItemsForRestaurant(int restaurantId);
    QSqlQuery getFoodItem(int foodId);

    // --- Orders ---
    QSqlQuery readAllOrders();
    QSqlQuery getOrdersForRestaurant(int restaurantId);
    QSqlQuery getOrderDetails(int orderId);
    QSqlQuery getOrderItems(int orderId);
    bool createNewOrder(const QJsonObject& orderData);
    bool addOrderItems(int orderId, const QMap<int, CartItem>& items);
    bool updateOrderStatus(int orderId, const QString& newStatus);
    bool markOrderAsReviewed(int orderId);
    bool createTestOrder(int restaurantId, int customerId, double totalPrice, const QString& status);

    // --- Cache Management ---
    bool clearRestaurantsTable();
    bool addRestaurant(const QJsonObject& restaurantData);

private:
    DataBaseHandler();
    QSqlDatabase m_db;
};

#endif // DATABASEHANDLER_H
