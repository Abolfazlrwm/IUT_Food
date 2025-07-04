#include "databasehandler.h"
#include <QFont>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDir>
DataBaseHandler::DataBaseHandler() {
    // استفاده از اتصال نام‌گذاری شده برای جلوگیری از تداخل
    if (QSqlDatabase::contains("main_connection")) {
        db = QSqlDatabase::database("main_connection");
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", "main_connection");
    }
}
bool DataBaseHandler::openDataBase(const QString& fileName) {
    db.setDatabaseName(fileName);
    if (!db.open()) {
        qDebug() << "DATABASE ERROR: Could not open database at path:" << fileName;
        qDebug() << "Error:" << db.lastError().text();
        return false;
    }
    QSqlQuery pragmaQuery(db);
    pragmaQuery.exec("PRAGMA encoding = 'UTF-8';");
    qDebug() << "DATABASE SUCCESS: Connected to database at:" << QDir(fileName).absolutePath();
    return true;
}
bool DataBaseHandler::createTables() {
    QSqlQuery q(db);

    // ساخت تمام جداول لازم با ستون‌های کامل از هر دو بخش
    q.exec("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE, password TEXT, role TEXT, is_active INTEGER DEFAULT 1, is_approved INTEGER DEFAULT 0, address TEXT)");
    q.exec("CREATE TABLE IF NOT EXISTS restaurants (id INTEGER PRIMARY KEY, name TEXT, type TEXT, location TEXT, price_range INTEGER)");
    q.exec("CREATE TABLE IF NOT EXISTS menu_items (id INTEGER PRIMARY KEY AUTOINCREMENT, restaurant_id INTEGER, name TEXT, description TEXT, price REAL, category TEXT, FOREIGN KEY(restaurant_id) REFERENCES restaurants(id))");
    q.exec("CREATE TABLE IF NOT EXISTS orders (id INTEGER PRIMARY KEY, customer_id INTEGER, restaurant_id INTEGER, status TEXT, total_price REAL, created_at TEXT, review_submitted INTEGER DEFAULT 0)");
    q.exec("CREATE TABLE IF NOT EXISTS order_items (id INTEGER PRIMARY KEY, order_id INTEGER, menu_item_id INTEGER, quantity INTEGER, price_per_item REAL, FOREIGN KEY(order_id) REFERENCES orders(id), FOREIGN KEY(menu_item_id) REFERENCES menu_items(id))");

    // اطمینان از وجود کاربر ادمین
    QSqlQuery checkAdmin(db);
    checkAdmin.prepare("SELECT COUNT(*) FROM users WHERE role = 'admin'");
    if (checkAdmin.exec() && checkAdmin.next() && checkAdmin.value(0).toInt() == 0) {
        QSqlQuery insertAdmin(db);
        insertAdmin.prepare("INSERT INTO users (username, password, role) VALUES ('admin', 'admin', 'admin')");
        insertAdmin.exec();
    }
    return true;
}


// User CRUD
bool DataBaseHandler::registerUser(const QString& userName, const QString& password, const QString& role) {
    QSqlQuery q(db);
    q.prepare("INSERT INTO users (username, password, role, is_active, is_approved) VALUES (?, ?, ?, 1, ?)");
    q.addBindValue(userName);
    q.addBindValue(password);
    q.addBindValue(role);
    q.addBindValue(role == "restaurant" ? 0 : 1);
    if (!q.exec()) { qDebug() << "Register user failed:" << q.lastError().text(); return false; }
    return true;
}
QSqlQuery DataBaseHandler::getMenuItemsForRestaurant(int restaurantId) {
    QSqlQuery q(db);
    q.prepare("SELECT * FROM menu_items WHERE restaurant_id = ?");
    q.addBindValue(restaurantId);
    q.exec();
    return q;
}
// User CRUD


QSqlQuery DataBaseHandler::readUser(const QString& userName) {
    QSqlQuery q(db);
    q.prepare("SELECT * FROM users WHERE username = ?");
    q.addBindValue(userName);
    q.exec();
    return q;
}

QSqlQuery DataBaseHandler::readAllUsers() {
    QSqlQuery q(db);
    q.exec("SELECT * FROM users");
    return q;
}

// databasehandler.cpp

QSqlQuery DataBaseHandler::readAllOrders()
{
    // از اتصال دیتابیس که در کانستراکتور ساخته شده، استفاده می‌کنیم
    QSqlQuery q(db);

    // تمام سفارش‌ها را می‌خوانیم و بر اساس ID (جدیدترین‌ها اول) مرتب می‌کنیم
    if (!q.exec("SELECT * FROM orders ORDER BY id DESC")) {
        qDebug() << "Failed to read all orders:" << q.lastError().text();
    }

    return q;
}
bool DataBaseHandler::clearRestaurantsTable()
{
    // از اتصال دیتابیس که در کانستراکتور ساخته شده، استفاده می‌کنیم
    QSqlQuery query(db);

    if (!query.exec("DELETE FROM restaurants")) {
        qDebug() << "Failed to clear restaurants table:" << query.lastError().text();
        return false;
    }

    qDebug() << "Local restaurants cache cleared successfully.";
    return true;
}

// Admin ops
bool DataBaseHandler::blockUser(int userId) {
    QSqlQuery q(QSqlDatabase::database("main_connection"));

    q.prepare("UPDATE users SET is_active = 0 WHERE id = ?");
    q.addBindValue(userId);
    return q.exec();
}

bool DataBaseHandler::unblockUser(int userId) {
    QSqlQuery q(QSqlDatabase::database("main_connection"));

    q.prepare("UPDATE users SET is_active = 1 WHERE id = ?");
    q.addBindValue(userId);
    return q.exec();
}

bool DataBaseHandler::approveRestaurant(int userId) {
    QSqlQuery q(QSqlDatabase::database("main_connection"));

    q.prepare("UPDATE users SET is_approved = 1 WHERE id = ? AND role = 'restaurant'");
    q.addBindValue(userId);
    return q.exec();
}

bool DataBaseHandler::disapproveRestaurant(int userId) {
    QSqlQuery q(QSqlDatabase::database("main_connection"));

    q.prepare("UPDATE users SET is_approved = 0 WHERE id = ? AND role = 'restaurant'");
    q.addBindValue(userId);
    return q.exec();
}

// Login
bool DataBaseHandler::loginUser(const QString& userName, const QString& password) {
    QSqlQuery q(QSqlDatabase::database("main_connection"));

    q.prepare("SELECT * FROM users WHERE username = ? AND password = ? AND is_active = 1");
    q.addBindValue(userName);
    q.addBindValue(password);
    q.exec();

    return q.next();
}

QString DataBaseHandler::getUserRole(const QString& username) {
    QSqlQuery q(QSqlDatabase::database("main_connection"));

    q.prepare("SELECT role FROM users WHERE username = ?");
    q.addBindValue(username);
    q.exec();
    if (q.next())
        return q.value(0).toString();
    return "";
}

bool DataBaseHandler::deleteUser(int userId) {
    QSqlQuery q(db);
    q.prepare("DELETE FROM users WHERE id = ?");
    q.addBindValue(userId);
    return q.exec();
}


// Orders





QSqlQuery DataBaseHandler::getAllRestaurants(const QString& typeFilter, const QString& locationFilter, const QString& nameFilter) {
    QSqlQuery q(db);
    QString queryString = "SELECT * FROM restaurants WHERE 1=1";
    if (!typeFilter.isEmpty() && typeFilter != "همه") queryString += " AND type = '" + typeFilter + "'";
    if (!locationFilter.isEmpty() && locationFilter != "همه") queryString += " AND location = '" + locationFilter + "'";
    if (!nameFilter.isEmpty()) queryString += " AND name LIKE '%" + nameFilter + "%'";
    q.prepare(queryString);
    q.exec();
    return q;
}
// این تابع یک رستوران جدید (که از سرور به صورت JSON آمده) را به جدول اضافه می‌کند
bool DataBaseHandler::addRestaurant(const QJsonObject& restaurantData)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO restaurants (id, name, type, location, price_range) "
                  "VALUES (:id, :name, :type, :location, :price_range)");

    query.bindValue(":id", restaurantData["id"].toInt());
    query.bindValue(":name", restaurantData["name"].toString());
    query.bindValue(":type", restaurantData["type"].toString());
    query.bindValue(":location", restaurantData["location"].toString());
    query.bindValue(":price_range", restaurantData["price_range"].toInt());

    if (!query.exec()) {
        qDebug() << "Failed to add restaurant:" << query.lastError().text();
        return false;
    }
    return true;
}
// databasehandler.cpp

bool DataBaseHandler::createNewOrder(const QJsonObject& orderData) {
    QSqlQuery query(db);
    query.prepare("INSERT INTO orders (id, customer_id, restaurant_id, status, total_price, created_at, review_submitted) "
                  "VALUES (:id, :customer_id, :restaurant_id, :status, :total_price, :created_at, 0)");
    query.bindValue(":id", orderData["id"].toInt());
    query.bindValue(":customer_id", orderData["customer_id"].toInt());
    query.bindValue(":restaurant_id", orderData["restaurant_id"].toInt());
    query.bindValue(":status", orderData["status"].toString());
    query.bindValue(":total_price", orderData["total_price"].toDouble());
    query.bindValue(":created_at", orderData["created_at"].toString());
    return query.exec();
}
QSqlQuery DataBaseHandler::getOrderDetails(int orderId) {
    QSqlQuery q(db);
    q.prepare("SELECT * FROM orders WHERE id = ?");
    q.addBindValue(orderId);
    q.exec();
    return q;
}

QSqlQuery DataBaseHandler::getOrderItems(int orderId) {
    QSqlQuery q;
    // با JOIN کردن، نام غذا را از جدول menu_items می‌گیریم
    q.prepare("SELECT mi.name, oi.quantity, oi.price_per_item "
              "FROM order_items oi "
              "JOIN menu_items mi ON oi.menu_item_id = mi.id "
              "WHERE oi.order_id = ?");
    q.addBindValue(orderId);
    q.exec();
    return q;
}
bool DataBaseHandler::updateOrderStatus(int orderId, const QString& newStatus)
{
    QSqlQuery query(db);
    query.prepare("UPDATE orders SET status = :status WHERE id = :order_id");

    query.bindValue(":status", newStatus);
    query.bindValue(":order_id", orderId);

    if (!query.exec()) {
        qDebug() << "Failed to update order status for order ID" << orderId << ":" << query.lastError().text();
        return false;
    }

    qDebug() << "Successfully updated status for order ID" << orderId << "to" << newStatus;
    return true;
}
#include <QVariant>

bool DataBaseHandler::addOrderItems(int orderId, const QMap<int, CartItem>& items)
{
    bool all_ok = true;
    QSqlQuery query(db);
    query.prepare("INSERT INTO order_items (order_id, menu_item_id, quantity, price_per_item) "
                  "VALUES (?, ?, ?, ?)");

    // حلقه روی تمام آیتم‌های موجود در سبد خرید
    for (const CartItem& item : items) {
        query.addBindValue(orderId);
        query.addBindValue(item.foodData["id"].toInt());
        query.addBindValue(item.quantity);
        query.addBindValue(item.foodData["price"].toDouble());

        if (!query.exec()) {
            qDebug() << "Failed to add order item:" << query.lastError().text();
            all_ok = false;
        }
    }
    return all_ok;
}
QSqlQuery DataBaseHandler::getUserDetails(int userId) {
    QSqlQuery q(db);
    q.prepare("SELECT username, address FROM users WHERE id = ?"); // فرض می‌کنیم ستون address وجود دارد
    q.addBindValue(userId);
    q.exec();
    return q;
}

bool DataBaseHandler::updateUserDetails(int userId, const QString& newUsername, const QString& newAddress) {
    QSqlQuery q(db);
    q.prepare("UPDATE users SET username = :username, address = :address WHERE id = :id");
    q.bindValue(":username", newUsername);
    q.bindValue(":address", newAddress);
    q.bindValue(":id", userId);

    if (!q.exec()) {
        qDebug() << "Failed to update user details:" << q.lastError().text();
        return false;
    }
    return true;
}

bool DataBaseHandler::markOrderAsReviewed(int orderId)
{
    QSqlQuery q(db);
    q.prepare("UPDATE orders SET review_submitted = 1 WHERE id = ?");
    q.addBindValue(orderId);
    return q.exec();
}
