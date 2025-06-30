#include "databasehandler.h"
#include <QFont>
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
    // databasehandler.cpp -> createTables()

    // ...
    bool ok3 = q.exec(
        "CREATE TABLE IF NOT EXISTS restaurants ("
        "id INTEGER PRIMARY KEY, name TEXT, type TEXT, "
        "location TEXT, price_range INTEGER)"); // <<< logo_url حذف شد

    // داده تستی بدون مسیر عکس

    // ...
    bool ok4 = q.exec(
        "CREATE TABLE IF NOT EXISTS menu_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "restaurant_id INTEGER,"
        "name TEXT,"
        "description TEXT,"
        "price REAL,"
        "category TEXT,"
        "FOREIGN KEY(restaurant_id) REFERENCES restaurants(id)" // تعریف کلید خارجی
        ")"
        );

    // اضافه کردن داده تستی برای منوی رستوران‌ها
    // منوی پیتزا پینو (restaurant_id = 1)

    bool ok5 = q.exec(
        "CREATE TABLE IF NOT EXISTS order_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "order_id INTEGER,"          // کلید خارجی به جدول orders
        "menu_item_id INTEGER,"     // کلید خارجی به جدول menu_items
        "quantity INTEGER,"         // تعداد سفارش داده شده از این آیتم
        "price_per_item REAL,"      // قیمت هر واحد در زمان خرید
        "FOREIGN KEY(order_id) REFERENCES orders(id),"
        "FOREIGN KEY(menu_item_id) REFERENCES menu_items(id)"
        ")"
        );
    // داده تستی برای آیتم‌های یک سفارش (مثلا برای سفارش با id=101 که قبلا ساختیم)
    // databasehandler.cpp -> createTables()

    // داده تستی با انکودینگ صحیح

    q.prepare("INSERT INTO restaurants (name, type, location, price_range) VALUES (?, ?, ?, ?)");
    q.addBindValue("رستوران نایب");
    q.addBindValue("ایرانی");
    q.addBindValue("تهران");
    q.addBindValue(3);

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


QSqlQuery DataBaseHandler::getAllRestaurants(const QString& typeFilter, const QString& locationFilter, const QString& nameFilter) {
    QSqlQuery q;
    QString queryString = "SELECT * FROM restaurants WHERE 1=1";

    if (!typeFilter.isEmpty() && typeFilter != "همه") {
        queryString += " AND type = '" + typeFilter + "'";
    }
    // کد جدید برای فیلتر منطقه
    if (!locationFilter.isEmpty() && locationFilter != "همه") {
        queryString += " AND location = '" + locationFilter + "'";
    }
    if (!nameFilter.isEmpty()) {
        queryString += " AND name LIKE '%" + nameFilter + "%'";
    }
    q.prepare(queryString);
    q.exec();
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
    query.prepare("INSERT INTO restaurants (id, name, type, location, price_range, logo_path) "
                  "VALUES (:id, :name, :type, :location, :price_range, :logo_path)");

    query.bindValue(":id", restaurantData["id"].toInt());
    query.bindValue(":name", restaurantData["name"].toString());
    query.bindValue(":type", restaurantData["type"].toString());
    query.bindValue(":location", restaurantData["location"].toString());
    query.bindValue(":price_range", restaurantData["price_range"].toInt());
    query.bindValue(":logo_path", restaurantData["logo_path"].toString());

    if (!query.exec()) {
        qDebug() << "Failed to add restaurant:" << query.lastError().text();
        return false;
    }
    return true;
}
bool DataBaseHandler::createNewOrder(const QJsonObject& orderData)
{
    QSqlQuery query;
    query.prepare("INSERT INTO orders (id, customer_id, restaurant_id, status, total_price, created_at) "
                  "VALUES (:id, :customer_id, :restaurant_id, :status, :total_price, :created_at)");

    query.bindValue(":id", orderData["id"].toInt());
    query.bindValue(":customer_id", orderData["customer_id"].toInt());
    query.bindValue(":restaurant_id", orderData["restaurant_id"].toInt());
    query.bindValue(":status", orderData["status"].toString());
    query.bindValue(":total_price", orderData["total_price"].toDouble());
    query.bindValue(":created_at", orderData["created_at"].toString());

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
