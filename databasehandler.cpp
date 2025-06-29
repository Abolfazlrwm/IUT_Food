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
    bool ok3 = q.exec(
        "CREATE TABLE IF NOT EXISTS restaurants ("
        "id INTEGER PRIMARY KEY, name TEXT, type TEXT, "
        "location TEXT, price_range INTEGER, logo_path TEXT)");
    //test
    q.exec("INSERT OR IGNORE INTO restaurants (id, name, type, location, price_range, logo_path) VALUES (1, 'پیتزا پینو', 'فست فود', 'مرداویج', 2, ':/icons/pizza.png')");
    q.exec("INSERT OR IGNORE INTO restaurants (id, name, type, location, price_range, logo_path) VALUES (2, 'رستوران شهرزاد', 'ایرانی', 'چهارباغ', 3, ':/icons/kebab.png')");
    q.exec("INSERT OR IGNORE INTO restaurants (id, name, type, location, price_range, logo_path) VALUES (3, 'کافه آنی', 'کافه', 'جلفا', 2, ':/icons/coffee.png')");
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
    q.exec("INSERT OR IGNORE INTO menu_items (restaurant_id, name, description, price, category) VALUES (1, 'پیتزا پپرونی', 'خمیر ایتالیایی، سس مخصوص، پپرونی تند', 280000, 'پیتزا')");
    q.exec("INSERT OR IGNORE INTO menu_items (restaurant_id, name, description, price, category) VALUES (1, 'نوشابه کوکا کولا', 'بطری خانواده', 30000, 'نوشیدنی')");

    // منوی رستوران شهرزاد (restaurant_id = 2)
    q.exec("INSERT OR IGNORE INTO menu_items (restaurant_id, name, description, price, category) VALUES (2, 'چلوکباب کوبیده', 'دو سیخ کباب با برنج ایرانی', 250000, 'غذای اصلی')");
    q.exec("INSERT OR IGNORE INTO menu_items (restaurant_id, name, description, price, category) VALUES (2, 'دوغ محلی', 'یک پارچ', 40000, 'نوشیدنی')");

    return ok1 && ok2 &&ok3 && ok4;
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

QSqlQuery DataBaseHandler::getAllRestaurants() {
    QSqlQuery q;
    q.exec("SELECT id, name, type, logo_path, price_range FROM restaurants");
    return q;
}
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
