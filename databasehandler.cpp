#include "databasehandler.h"

DataBaseHandler& DataBaseHandler::instance()
{
    static DataBaseHandler instance;
    return instance;
}
DataBaseHandler::DataBaseHandler()
{
    initializeDatabase();
}

bool DataBaseHandler::initializeDatabase()
{
    // پیاده‌سازی تابع
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(path);
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(path + "/restaurant.db");

    if (!m_db.open()) {
        qDebug() << "Error opening database:" << m_db.lastError().text();
        return false;
    }

    return createTables();
}
bool DataBaseHandler::validateRestaurant(int restaurantId)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM users WHERE id = ? AND role = 'restaurant' AND is_approved = 1");
    query.addBindValue(restaurantId);

    if (!query.exec()) {
        qDebug() << "Validation error:" << query.lastError().text();
        return false;
    }

    return query.next();
}
bool DataBaseHandler::isOpen() const
{
    return m_db.isOpen();
}
bool DataBaseHandler::createTestOrder(int restaurantId, int customerId, double totalPrice, const QString& status)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO orders (restaurant_id, customer_id, total_price, status, created_at) "
              "VALUES (?, ?, ?, ?, datetime('now'))");
    q.addBindValue(restaurantId);
    q.addBindValue(customerId);
    q.addBindValue(totalPrice);
    q.addBindValue(status);

    if (!q.exec()) {
        qDebug() << "Create test order failed:" << q.lastError().text();
        return false;
    }
    return true;
}
// تابع باز کردن فایل دیتابیس
bool DataBaseHandler::openDataBase(const QString& fileName) {
    if (m_db.isOpen()) {
        m_db.close();
    }
    m_db.setDatabaseName(fileName);
    if (!m_db.open()) {
        qDebug() << "Error opening DB:" << m_db.lastError().text();
        return false;
    }
    QSqlQuery query(m_db);
    query.exec("PRAGMA foreign_keys = ON");

    qDebug() << "Database opened successfully at:" << fileName;
    return true;
}

// تابع ساخت جداول اولیه در دیتابیس
bool DataBaseHandler::createTables() {
    QSqlQuery q(m_db);

    // ایجاد جدول users اگر وجود ندارد
    bool ok_users = q.exec("CREATE TABLE IF NOT EXISTS users ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "username TEXT UNIQUE NOT NULL,"
                           "password TEXT NOT NULL,"
                           "name TEXT,"
                           "role TEXT NOT NULL,"
                           "is_active INTEGER DEFAULT 1,"
                           "is_approved INTEGER DEFAULT 0,"
                           "restaurant_type TEXT,"
                           "location TEXT,"
                           "price_range INTEGER,"
                           "logo_path TEXT)");

    // ایجاد جدول foods با محدودیت FOREIGN KEY
    bool ok_foods = q.exec("CREATE TABLE IF NOT EXISTS foods ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "restaurant_id INTEGER NOT NULL,"
                           "name TEXT NOT NULL,"
                           "price REAL NOT NULL,"
                           "description TEXT,"
                           "FOREIGN KEY(restaurant_id) REFERENCES users(id) ON DELETE CASCADE)");

    if(!ok_foods) {
        qDebug() << "Foods table error:" << q.lastError().text();
    }

    return ok_users && ok_foods;
}
int DataBaseHandler::getOrderCountForRestaurant(int restaurantId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM orders WHERE restaurant_id = ?");
    q.addBindValue(restaurantId);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return -1;
}
int DataBaseHandler::getUserId(const QString& username) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM users WHERE username = ?");
    q.addBindValue(username);
    if (q.exec() && q.next()) {
        return q.value("id").toInt();
    } else {
        qDebug() << "User not found:" << username << q.lastError().text();
        return -1;
    }
}

// تابع ثبت‌نام یک رستوران جدید
bool DataBaseHandler::registerRestaurant(const QString& username, const QString& password, const QString& name, const QString& type, const QString& location, int priceRange) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO users (username, password, role, name, is_approved, restaurant_type, location, price_range) "
              "VALUES (?, ?, 'restaurant', ?, 0, ?, ?, ?)");
    q.addBindValue(username);
    q.addBindValue(password);
    q.addBindValue(name);
    q.addBindValue(type);
    q.addBindValue(location);
    q.addBindValue(priceRange);

    if (!q.exec()) {
        qDebug() << "Register restaurant failed:" << q.lastError().text();
        return false;
    }
    return true;
}

// تابع ورود کاربر
bool DataBaseHandler::loginUser(const QString& userName, const QString& password, QString& role, int& userId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id, role FROM users WHERE username = ? AND password = ? AND is_active = 1 AND is_approved = 1");
    q.addBindValue(userName);
    q.addBindValue(password);

    if (q.exec() && q.next()) {
        userId = q.value("id").toInt();
        role = q.value("role").toString();
        return true;
    }
    return false;
}

// تابع خواندن اطلاعات یک کاربر بر اساس نام کاربری
QSqlQuery DataBaseHandler::readUser(const QString& username) {
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM users WHERE username = ?");
    q.addBindValue(username);
    q.exec();
    return q;
}

// تابع تایید کردن یک رستوران (برای ادمین)
bool DataBaseHandler::approveRestaurant(int userId) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET is_approved = 1 WHERE id = ? AND role = 'restaurant'");
    q.addBindValue(userId);
    return q.exec();
}

// تابع خواندن اطلاعات پروفایل یک رستوران
QSqlQuery DataBaseHandler::readRestaurantDetails(int restaurantId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT name, restaurant_type, location, price_range FROM users WHERE id = ?");
    q.addBindValue(restaurantId);
    q.exec();
    return q;
}

// تابع به‌روزرسانی اطلاعات پروفایل یک رستوران
bool DataBaseHandler::updateRestaurantDetails(int restaurantId, const QString& name,
                                              const QString& type, const QString& location,
                                              int priceRange) {
    if (!m_db.isOpen()) {
        qDebug() << "Database is not open!";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET name = :name, restaurant_type = :type, "
              "location = :location, price_range = :price_range "
              "WHERE id = :id");

    q.bindValue(":name", name);
    q.bindValue(":type", type);
    q.bindValue(":location", location);
    q.bindValue(":price_range", priceRange);
    q.bindValue(":id", restaurantId);

    if (!q.exec()) {
        qDebug() << "Update failed:" << q.lastError().text()
        << "Query:" << q.lastQuery();
        return false;
    }
    return true;
}
void DataBaseHandler::checkDatabaseStatus() {
    qDebug() << "Database status:";
    qDebug() << "- Is open:" << m_db.isOpen();
    qDebug() << "- Last error:" << m_db.lastError().text();
    qDebug() << "- Tables:" << m_db.tables();

    if (m_db.isOpen()) {
        QSqlQuery q("SELECT name FROM sqlite_master WHERE type='table'", m_db);
        while (q.next()) {
            qDebug() << "Table:" << q.value(0).toString();
        }
    }
}
// تابع افزودن یک غذای جدید به منو
bool DataBaseHandler::addFoodItem(int restaurantId, const QString& name,
                                  double price, const QString& description)
{
    if (!m_db.isOpen()) {
        qDebug() << "Database is not open!";
        return false;
    }

    // اعتبارسنجی ورودی‌ها
    if (name.isEmpty() || price <= 0 || !validateRestaurant(restaurantId)) {
        qDebug() << "Invalid food data or restaurant doesn't exist";
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO foods (restaurant_id, name, price, description) "
                  "VALUES (:restaurant_id, :name, :price, :description)");

    query.bindValue(":restaurant_id", restaurantId);
    query.bindValue(":name", name);
    query.bindValue(":price", price);
    query.bindValue(":description", description);

    if (!query.exec()) {
        qDebug() << "Error adding food item -" << query.lastError().text();
        qDebug() << "Last query:" << query.lastQuery();
        return false;
    }

    return true;
}
// تابع ویرایش یک غذای موجود
bool DataBaseHandler::updateFoodItem(int foodId, const QString& name, double price, const QString& description)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE foods SET name = ?, price = ?, description = ? WHERE id = ?");
    q.addBindValue(name);
    q.addBindValue(price);
    q.addBindValue(description);
    q.addBindValue(foodId);

    if (!q.exec()) {
        qDebug() << "Update food item failed:" << q.lastError();
        return false;
    }
    return q.numRowsAffected() > 0;
}

// تابع حذف یک غذا از منو
bool DataBaseHandler::deleteFoodItem(int foodId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM foods WHERE id = ?");
    q.addBindValue(foodId);

    if (!q.exec()) {
        qDebug() << "Delete food item failed:" << q.lastError();
        return false;
    }
    return q.numRowsAffected() > 0;
}
// تابع گرفتن تمام آیتم‌های منوی یک رستوران
QVector<FoodItem> DataBaseHandler::getMenuForRestaurant(int restaurantId)
{
    QVector<FoodItem> menu;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, name, price, description FROM foods WHERE restaurant_id = ?");
    q.addBindValue(restaurantId);

    if(q.exec()) {
        while(q.next()) {
            FoodItem item;
            item.id = q.value("id").toInt();
            item.name = q.value("name").toString();
            item.price = q.value("price").toDouble();
            item.description = q.value("description").toString();
            menu.append(item);
        }
    }
    return menu;
}
// تابع گرفتن اطلاعات یک غذای خاص با شناسه آن
QSqlQuery DataBaseHandler::getFoodItem(int foodId)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id, name, price, description FROM foods WHERE id = ?");
    q.addBindValue(foodId);
    q.exec();
    return q;
}

// تابع گرفتن سفارش‌های یک رستوران
QSqlQuery DataBaseHandler::getOrdersForRestaurant(int restaurantId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT o.id, o.status, o.total_price, o.created_at, u.username "
              "FROM orders o JOIN users u ON o.customer_id = u.id "
              "WHERE o.restaurant_id = ? ORDER BY o.created_at DESC");
    q.addBindValue(restaurantId);
    q.exec();
    return q;
}

// تابع به‌روزرسانی وضعیت یک سفارش
bool DataBaseHandler::updateOrderStatus(int orderId, const QString& status) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE orders SET status = ? WHERE id = ?");
    q.addBindValue(status);
    q.addBindValue(orderId);
    if (!q.exec()) {
        qDebug() << "Update order status failed:" << q.lastError();
        return false;
    }
    return true;
}
void DataBaseHandler::logDatabaseStatus()
{
    qDebug() << "Database status:";
    qDebug() << "- Open:" << m_db.isOpen();
    qDebug() << "- Tables:" << m_db.tables();
    qDebug() << "- Last error:" << m_db.lastError().text();

    QSqlQuery q("PRAGMA foreign_keys", m_db);
    if (q.exec() && q.next()) {
        qDebug() << "- Foreign keys enabled:" << q.value(0).toBool();
    }
}
