#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QString>
class DataBaseHandler
{
public:
    bool openDataBase(QString fileName);
   // bool createTables();
    //crud for users
    bool registerUser(QString name, QString username, QString password,QString city, QString cityArea);
    QSqlQuery readUser(QString userName);
    QSqlQuery readAllUsers();
    bool deleteUser(QString userName);
    bool deleteUser(int userId);
    /* should fix restaurant about how it registers
    bool registerRestaurant()*/
    // there  is a default admin
    // only admin can register another admin


    bool loginUser(QString userName, QString password);
    QString getUserRole(QString username);


    DataBaseHandler();
};

#endif // DATABASEHANDLER_H
