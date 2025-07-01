#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QString>
#include <QList>
#include <QSqlError>

class DataBaseHandler
{
private:
    QSqlDatabase db;

public:
    DataBaseHandler();

    // Connect and schema
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
};

#endif // DATABASEHANDLER_H
