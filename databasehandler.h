#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonObject>
#include <QMap>
#include "datatypes.h" // برای دسترسی به struct CartItem

class DataBaseHandler
{
public:
    DataBaseHandler();

    // توابع اصلی اتصال و ساخت جداول
    bool openDataBase(const QString& fileName);
    bool createTables();

    // --- توابع مدیریت کاربر و ادمین ---
    bool registerUser(const QString& userName, const QString& password, const QString& role);
    QSqlQuery readUser(const QString& userName);
    QSqlQuery readAllUsers();
    bool deleteUser(const QString& userName);
    bool deleteUser(int userId);
    bool blockUser(int userId);
    bool unblockUser(int userId);
    bool approveRestaurant(int userId);
    bool disapproveRestaurant(int userId);
    bool loginUser(const QString& userName, const QString& password);
    QString getUserRole(const QString& username);
    QSqlQuery getUserDetails(int userId);
    bool updateUserDetails(int userId, const QString& newUsername, const QString& newAddress);

    // --- توابع مدیریت رستوران و سفارشات (بخش مشتری) ---
    QSqlQuery getAllRestaurants(const QString& typeFilter, const QString& locationFilter, const QString& nameFilter);
    QSqlQuery getMenuItemsForRestaurant(int restaurantId);
    QSqlQuery readAllOrders();
    QSqlQuery getOrderDetails(int orderId);
    QSqlQuery getOrderItems(int orderId);
    bool createNewOrder(const QJsonObject& orderData);
    bool addOrderItems(int orderId, const QMap<int, CartItem>& items);
    bool updateOrderStatus(int orderId, const QString& newStatus);
    bool markOrderAsReviewed(int orderId);


    // --- توابع مدیریت کش ---
    bool clearRestaurantsTable();
    bool addRestaurant(const QJsonObject& restaurantData);

private:
    QSqlDatabase db;
};

#endif // DATABASEHANDLER_H
