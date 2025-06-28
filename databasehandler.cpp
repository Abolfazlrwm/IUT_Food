#include "databasehandler.h"

DataBaseHandler::DataBaseHandler() {
    db = QSqlDatabase::addDatabase("QSQLITE");
}

bool DataBaseHandler::openDataBase(const QString& fileName) {
    db.setDatabaseName(fileName);
    if (!db.open()) {
        qDebug() << "Error opening DB:" << db.lastError().text();
        return false;
    }
    return true;
}

bool DataBaseHandler::createTables() {
    QSqlQuery q;
    bool ok1 = q.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE,"
        "password TEXT,"
        "role TEXT,"
        "is_active INTEGER DEFAULT 1,"
        "is_approved INTEGER DEFAULT 0"
        ")"
        );

    bool ok2 = q.exec(
        "CREATE TABLE IF NOT EXISTS orders ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "customer_id INTEGER,"
        "restaurant_id INTEGER,"
        "status TEXT,"
        "total_price REAL,"
        "created_at TEXT"
        ")"
        );

    return ok1 && ok2;
}

// User CRUD
bool DataBaseHandler::registerUser(const QString& userName, const QString& password, const QString& role) {
    QSqlQuery q;
    q.prepare("INSERT INTO users (username, password, role, is_active, is_approved) "
              "VALUES (?, ?, ?, 1, ?)");
    q.addBindValue(userName);
    q.addBindValue(password);
    q.addBindValue(role);
    q.addBindValue(role == "restaurant" ? 0 : 1);

    if (!q.exec()) {
        qDebug() << "Register user failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QSqlQuery DataBaseHandler::readUser(const QString& userName) {
    QSqlQuery q;
    q.prepare("SELECT * FROM users WHERE username = ?");
    q.addBindValue(userName);
    q.exec();
    return q;
}

QSqlQuery DataBaseHandler::readAllUsers() {
    QSqlQuery q;
    q.exec("SELECT * FROM users");
    return q;
}

bool DataBaseHandler::deleteUser(const QString& userName) {
    QSqlQuery q;
    q.prepare("DELETE FROM users WHERE username = ?");
    q.addBindValue(userName);
    return q.exec();
}

bool DataBaseHandler::deleteUser(int userId) {
    QSqlQuery q;
    q.prepare("DELETE FROM users WHERE id = ?");
    q.addBindValue(userId);
    return q.exec();
}

// Admin ops
bool DataBaseHandler::blockUser(int userId) {
    QSqlQuery q;
    q.prepare("UPDATE users SET is_active = 0 WHERE id = ?");
    q.addBindValue(userId);
    return q.exec();
}

bool DataBaseHandler::unblockUser(int userId) {
    QSqlQuery q;
    q.prepare("UPDATE users SET is_active = 1 WHERE id = ?");
    q.addBindValue(userId);
    return q.exec();
}

bool DataBaseHandler::approveRestaurant(int userId) {
    QSqlQuery q;
    q.prepare("UPDATE users SET is_approved = 1 WHERE id = ? AND role = 'restaurant'");
    q.addBindValue(userId);
    return q.exec();
}

bool DataBaseHandler::disapproveRestaurant(int userId) {
    QSqlQuery q;
    q.prepare("UPDATE users SET is_approved = 0 WHERE id = ? AND role = 'restaurant'");
    q.addBindValue(userId);
    return q.exec();
}

// Login
bool DataBaseHandler::loginUser(const QString& userName, const QString& password) {
    QSqlQuery q;
    q.prepare("SELECT * FROM users WHERE username = ? AND password = ? AND is_active = 1");
    q.addBindValue(userName);
    q.addBindValue(password);
    q.exec();

    return q.next();
}

QString DataBaseHandler::getUserRole(const QString& username) {
    QSqlQuery q;
    q.prepare("SELECT role FROM users WHERE username = ?");
    q.addBindValue(username);
    q.exec();
    if (q.next())
        return q.value(0).toString();
    return "";
}

// Orders
QSqlQuery DataBaseHandler::readAllOrders() {
    QSqlQuery q;
    q.exec("SELECT * FROM orders");
    return q;
}
