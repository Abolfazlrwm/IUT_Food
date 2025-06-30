#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QString>
#include <QList>
#include <QSqlError>
#include <QJsonObject>

class DataBaseHandler
{
private:
    QSqlDatabase db;

public:
    DataBaseHandler();

    // Connect and schema
    bool openDataBase(const QString& fileName);
    bool createTables();

    // User CRUD
    bool registerUser(const QString& userName, const QString& password, const QString& role);
    QSqlQuery readUser(const QString& userName);
    QSqlQuery readAllUsers();
    bool deleteUser(const QString& userName);
    bool deleteUser(int userId);

    // Admin-specific operations
    bool blockUser(int userId);
    bool unblockUser(int userId);
    bool approveRestaurant(int userId);
    bool disapproveRestaurant(int userId);

    // Login
    bool loginUser(const QString& userName, const QString& password);
    QString getUserRole(const QString& username);

    // Orders
    QSqlQuery readAllOrders();

    //restaurants
    QSqlQuery getAllRestaurants(const QString& typeFilter, const QString& locationFilter, const QString& nameFilter);
    QSqlQuery getMenuItemsForRestaurant(int restaurantId);

    //cache
    bool clearRestaurantsTable();
    bool addRestaurant(const QJsonObject& restaurantData);
    bool createNewOrder(const QJsonObject& orderData); // <<< برای ثبت سفارش جدید

};

#endif // DATABASEHANDLER_H
