#include "databasehandler.h"
#include "QDir"
#include "QSqlQuery"
#include <QMessageBox>

DataBaseHandler::DataBaseHandler() {}

bool DataBaseHandler::openDataBase(QString fileNamePath){
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    QString dbPath = fileNamePath;
    db.setDatabaseName(dbPath);
    db.open();
    if(!db.isOpen()){
        //QDebug << "database didnt open";
        return false;
    }
    return true;

}
bool DataBaseHandler::registerUser(QString name, QString username, QString password, QString city, QString cityArea){
    QSqlQuery query;
    query.prepare("INSERT INTO users(Name, username, password, City, City_Area)VALUES(?,?,?,?,?");
    query.addBindValue(name);
    query.addBindValue(username);
    query.addBindValue(password);
    query.addBindValue(city);
    query.addBindValue(cityArea);

    if(query.exec()){
       // QMessageBox::(this, "Success", "User is made successfully");
        return true;
    }
    else{
        //QMessageBox::warning(this, "Error", "User is not made successfully");
        return false;
    }
}


