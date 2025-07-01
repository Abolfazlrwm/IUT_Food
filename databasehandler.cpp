#include "databasehandler.h"
#include <QFont>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDir>
DataBaseHandler::DataBaseHandler() {
    db = QSqlDatabase::addDatabase("QSQLITE");
}

bool DataBaseHandler::openDataBase(const QString& fileName) {
    db.setDatabaseName(fileName);
    if (!db.open()) {
        qDebug() << "DATABASE ERROR: Could not open database at path:" << fileName;
        qDebug() << "Error:" << db.lastError().text();
        return false;
    }

    // این دستور به اتصال دیتابیس می‌گوید که از انکودینگ UTF-8 استفاده کند
    QSqlQuery pragmaQuery(db);
    if (!pragmaQuery.exec("PRAGMA encoding = 'UTF-8';")) {
        qDebug() << "DATABASE WARNING: Failed to set UTF-8 encoding:" << pragmaQuery.lastError().text();
    }

    // مسیر دقیق فایل دیتابیس را در کنسول چاپ می‌کنیم
    qDebug() << "DATABASE SUCCESS: Connected to database at:" << QDir(fileName).absolutePath();

    return true;
}

bool DataBaseHandler::createTables() {
    QSqlQuery q;
    bool ok1 = q.exec("CREATE TABLE IF NOT EXISTS users ("
                      "id INTEGER PRIMARY KEY, username TEXT, password TEXT, role TEXT, address TEXT)");

    bool ok2 = q.exec("CREATE TABLE IF NOT EXISTS restaurants ("
                      "id INTEGER PRIMARY KEY, name TEXT, type TEXT, location TEXT, price_range INTEGER)");

    // ...
    bool ok3 = q.exec("CREATE TABLE IF NOT EXISTS menu_items ("
                      "id INTEGER PRIMARY KEY, restaurant_id INTEGER, name TEXT, description TEXT, price REAL, category TEXT, "
                      "FOREIGN KEY(restaurant_id) REFERENCES restaurants(id))");

    // داده تستی بدون مسیر عکس

    // ...
    bool ok4 = q.exec("CREATE TABLE IF NOT EXISTS orders ("
                      "id INTEGER PRIMARY KEY, customer_id INTEGER, restaurant_id INTEGER, status TEXT, "
                      "total_price REAL, created_at TEXT, review_submitted INTEGER DEFAULT 0)");

    // اضافه کردن داده تستی برای منوی رستوران‌ها
    // منوی پیتزا پینو (restaurant_id = 1)

    bool ok5 = q.exec("CREATE TABLE IF NOT EXISTS order_items ("
                      "id INTEGER PRIMARY KEY, order_id INTEGER, menu_item_id INTEGER, quantity INTEGER, price_per_item REAL, "
                      "FOREIGN KEY(order_id) REFERENCES orders(id), FOREIGN KEY(menu_item_id) REFERENCES menu_items(id))");
    // داده تستی برای آیتم‌های یک سفارش (مثلا برای سفارش با id=101 که قبلا ساختیم)
    // databasehandler.cpp -> createTables()

    // داده تستی با انکودینگ صحیح



    if (!q.exec()) {
        qDebug() << "Insert error:" << q.lastError().text();
    }

    return ok1 && ok2 && ok3 && ok4 && ok5; // <<< ok5 را اضافه کنید

}
QSqlQuery DataBaseHandler::getMenuItemsForRestaurant(int restaurantId) {
    QSqlQuery q;
    // کوئری فقط آیتم‌هایی را انتخاب می‌کند که restaurant_id آنها با ورودی ما برابر باشد
    q.prepare("SELECT * FROM menu_items WHERE restaurant_id = ?");
    q.addBindValue(restaurantId);
    q.exec();
    return q;
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
//restaurants


// databasehandler.cpp

QSqlQuery DataBaseHandler::getAllRestaurants(const QString& typeFilter, const QString& locationFilter, const QString& nameFilter) {
    QSqlQuery q;
    QString queryString = "SELECT * FROM restaurants WHERE 1=1";

    // استفاده از QVariantMap برای نگهداری مقادیری که باید bind شوند
    QVariantMap bindings;

    if (!typeFilter.isEmpty() && typeFilter != "همه") {
        queryString += " AND type = :type";
        bindings[":type"] = typeFilter;
    }
    if (!locationFilter.isEmpty() && locationFilter != "همه") {
        queryString += " AND location = :location";
        bindings[":location"] = locationFilter;
    }
    if (!nameFilter.isEmpty()) {
        queryString += " AND name LIKE :name";
        bindings[":name"] = "%" + nameFilter + "%";
    }

    q.prepare(queryString);

    // Bind کردن تمام مقادیر
    for(auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
        q.bindValue(it.key(), it.value());
    }

    if(!q.exec()) {
        qDebug() << "Failed to execute getAllRestaurants query:" << q.lastError().text();
    }

    return q;
}
bool DataBaseHandler::clearRestaurantsTable()
{
    QSqlQuery query;
    if (!query.exec("DELETE FROM restaurants")) {
        qDebug() << "Failed to clear restaurants table:" << query.lastError().text();
        return false;
    }
    return true;
}

// این تابع یک رستوران جدید (که از سرور به صورت JSON آمده) را به جدول اضافه می‌کند
bool DataBaseHandler::addRestaurant(const QJsonObject& restaurantData)
{
    QSqlQuery query;
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

bool DataBaseHandler::createNewOrder(const QJsonObject& orderData)
{
    QSqlQuery query;
    // === دستور INSERT صحیح با ۷ پارامتر ===
    query.prepare("INSERT INTO orders (id, customer_id, restaurant_id, status, total_price, created_at, review_submitted) "
                  "VALUES (:id, :customer_id, :restaurant_id, :status, :total_price, :created_at, :review_submitted)");

    query.bindValue(":id", orderData["id"].toInt());
    query.bindValue(":customer_id", orderData["customer_id"].toInt());
    query.bindValue(":restaurant_id", orderData["restaurant_id"].toInt());
    query.bindValue(":status", orderData["status"].toString());
    query.bindValue(":total_price", orderData["total_price"].toDouble());
    query.bindValue(":created_at", orderData["created_at"].toString());
    query.bindValue(":review_submitted", 0); // مقدار پیش‌فرض برای ستون جدید

    if (!query.exec()) {
        qDebug() << "Failed to create new order in local DB:" << query.lastError().text();
        return false;
    }
    return true;
}
QSqlQuery DataBaseHandler::getOrderDetails(int orderId) {
    QSqlQuery q;
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
    QSqlQuery query;
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

// این تابع را به انتهای databasehandler.cpp اضافه کنید
bool DataBaseHandler::addOrderItems(int orderId, const QMap<int, CartItem>& items)
{
    bool all_ok = true;
    QSqlQuery query;
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
    QSqlQuery q;
    q.prepare("SELECT username, address FROM users WHERE id = ?"); // فرض می‌کنیم ستون address وجود دارد
    q.addBindValue(userId);
    q.exec();
    return q;
}

bool DataBaseHandler::updateUserDetails(int userId, const QString& newUsername, const QString& newAddress) {
    QSqlQuery q;
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
