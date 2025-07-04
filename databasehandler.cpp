#include "databasehandler.h"
#include <QFont>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QThread>
#include <QCoreApplication>
#include <QStandardPaths>

// --- Singleton Instance ---
DataBaseHandler& DataBaseHandler::instance() {
    static DataBaseHandler instance;
    return instance;
}

// --- Constructor / Initialization ---
DataBaseHandler::DataBaseHandler() {
    // For singleton: only initialize once
    initializeDatabase();
}

bool DataBaseHandler::initializeDatabase() {
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

bool DataBaseHandler::isOpen() const {
    return m_db.isOpen();
}

// --- Database Connection & Table Creation ---
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

bool DataBaseHandler::createTables() {
    QSqlQuery q(m_db);

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
                           "logo_path TEXT,"
                           "address TEXT"
                           ")");

    bool ok_restaurants = q.exec("CREATE TABLE IF NOT EXISTS restaurants ("
                                 "id INTEGER PRIMARY KEY,"
                                 "name TEXT,"
                                 "type TEXT,"
                                 "location TEXT,"
                                 "price_range INTEGER"
                                 ")");

    bool ok_menu_items = q.exec("CREATE TABLE IF NOT EXISTS menu_items ("
                                "id INTEGER PRIMARY KEY,"
                                "restaurant_id INTEGER,"
                                "name TEXT,"
                                "description TEXT,"
                                "price REAL,"
                                "category TEXT,"
                                "FOREIGN KEY(restaurant_id) REFERENCES restaurants(id))");

    bool ok_foods = q.exec("CREATE TABLE IF NOT EXISTS foods ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           "restaurant_id INTEGER NOT NULL,"
                           "name TEXT NOT NULL,"
                           "price REAL NOT NULL,"
                           "description TEXT,"
                           "FOREIGN KEY(restaurant_id) REFERENCES users(id) ON DELETE CASCADE)");

    bool ok_orders = q.exec("CREATE TABLE IF NOT EXISTS orders ("
                            "id INTEGER PRIMARY KEY,"
                            "customer_id INTEGER,"
                            "restaurant_id INTEGER,"
                            "status TEXT,"
                            "total_price REAL,"
                            "created_at TEXT,"
                            "review_submitted INTEGER DEFAULT 0"
                            ")");

    bool ok_order_items = q.exec("CREATE TABLE IF NOT EXISTS order_items ("
                                 "id INTEGER PRIMARY KEY,"
                                 "order_id INTEGER,"
                                 "menu_item_id INTEGER,"
                                 "quantity INTEGER,"
                                 "price_per_item REAL,"
                                 "FOREIGN KEY(order_id) REFERENCES orders(id),"
                                 "FOREIGN KEY(menu_item_id) REFERENCES menu_items(id))");

    if(!ok_foods) {
        qDebug() << "Foods table error:" << q.lastError().text();
    }

    return ok_users && ok_restaurants && ok_menu_items && ok_foods && ok_orders && ok_order_items;
}

// --- User/Admin Management ---
bool DataBaseHandler::registerUser(const QString& userName, const QString& password, const QString& role) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO users (username, password, role, is_active, is_approved) VALUES (?, ?, ?, 1, ?)");
    q.addBindValue(userName);
    q.addBindValue(password);
    q.addBindValue(role);
    q.addBindValue(role == "restaurant" ? 0 : 1);
    if (!q.exec()) { qDebug() << "Register user failed:" << q.lastError().text(); return false; }
    return true;
}
QSqlQuery DataBaseHandler::readUser(const QString& userName) {
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM users WHERE username = ?");
    q.addBindValue(userName);
    q.exec();
    return q;
}
QSqlQuery DataBaseHandler::readAllUsers() {
    QSqlQuery q(m_db);
    q.exec("SELECT * FROM users");
    return q;
}
bool DataBaseHandler::deleteUser(const QString& userName) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM users WHERE username = ?");
    q.addBindValue(userName);
    return q.exec();
}
bool DataBaseHandler::deleteUser(int userId) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM users WHERE id = ?");
    q.addBindValue(userId);
    return q.exec();
}
bool DataBaseHandler::blockUser(int userId) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET is_active = 0 WHERE id = ?");
    q.addBindValue(userId);
    return q.exec();
}
bool DataBaseHandler::unblockUser(int userId) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET is_active = 1 WHERE id = ?");
    q.addBindValue(userId);
    return q.exec();
}
bool DataBaseHandler::approveRestaurant(int userId) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET is_approved = 1 WHERE id = ? AND role = 'restaurant'");
    q.addBindValue(userId);
    return q.exec();
}
bool DataBaseHandler::disapproveRestaurant(int userId) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET is_approved = 0 WHERE id = ? AND role = 'restaurant'");
    q.addBindValue(userId);
    return q.exec();
}

bool DataBaseHandler::loginUser(const QString& userName, const QString& password) {
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM users WHERE username = ? AND password = ? AND is_active = 1");
    q.addBindValue(userName);
    q.addBindValue(password);
    q.exec();
    return q.next();
}
// Extended login with role and id output
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
QString DataBaseHandler::getUserRole(const QString& username) {
    QSqlQuery q(m_db);
    q.prepare("SELECT role FROM users WHERE username = ?");
    q.addBindValue(username);
    q.exec();
    if (q.next())
        return q.value(0).toString();
    return "";
}
QSqlQuery DataBaseHandler::getUserDetails(int userId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT username, address FROM users WHERE id = ?");
    q.addBindValue(userId);
    q.exec();
    return q;
}
bool DataBaseHandler::updateUserDetails(int userId, const QString& newUsername, const QString& newAddress) {
    QSqlQuery q(m_db);
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

// --- Restaurant Management ---
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
bool DataBaseHandler::validateRestaurant(int restaurantId) {
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM users WHERE id = ? AND role = 'restaurant' AND is_approved = 1");
    query.addBindValue(restaurantId);
    if (!query.exec()) {
        qDebug() << "Validation error:" << query.lastError().text();
        return false;
    }
    return query.next();
}
QSqlQuery DataBaseHandler::readRestaurantDetails(int restaurantId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT name, restaurant_type, location, price_range FROM users WHERE id = ?");
    q.addBindValue(restaurantId);
    q.exec();
    return q;
}
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
int DataBaseHandler::getOrderCountForRestaurant(int restaurantId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM orders WHERE restaurant_id = ?");
    q.addBindValue(restaurantId);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return -1;
}
QSqlQuery DataBaseHandler::getAllRestaurants(const QString& typeFilter, const QString& locationFilter, const QString& nameFilter) {
    QSqlQuery q(m_db);
    QString queryString = "SELECT * FROM restaurants WHERE 1=1";
    if (!typeFilter.isEmpty() && typeFilter != "همه") queryString += " AND type = '" + typeFilter + "'";
    if (!locationFilter.isEmpty() && locationFilter != "همه") queryString += " AND location = '" + locationFilter + "'";
    if (!nameFilter.isEmpty()) queryString += " AND name LIKE '%" + nameFilter + "%'";
    q.prepare(queryString);
    q.exec();
    return q;
}
bool DataBaseHandler::clearRestaurantsTable() {
    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM restaurants")) {
        qDebug() << "Failed to clear restaurants table:" << query.lastError().text();
        return false;
    }
    qDebug() << "Local restaurants cache cleared successfully.";
    return true;
}
bool DataBaseHandler::addRestaurant(const QJsonObject& restaurantData) {
    QSqlQuery query(m_db);
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

// --- Menu Items / Food Management ---
bool DataBaseHandler::addFoodItem(int restaurantId, const QString& name, double price, const QString& description) {
    if (!m_db.isOpen()) {
        qDebug() << "Database is not open!";
        return false;
    }
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
bool DataBaseHandler::updateFoodItem(int foodId, const QString& name, double price, const QString& description) {
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
bool DataBaseHandler::deleteFoodItem(int foodId) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM foods WHERE id = ?");
    q.addBindValue(foodId);

    if (!q.exec()) {
        qDebug() << "Delete food item failed:" << q.lastError();
        return false;
    }
    return q.numRowsAffected() > 0;
}
QVector<FoodItem> DataBaseHandler::getMenuForRestaurant(int restaurantId) {
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
QSqlQuery DataBaseHandler::getMenuItemsForRestaurant(int restaurantId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM menu_items WHERE restaurant_id = ?");
    q.addBindValue(restaurantId);
    q.exec();
    return q;
}
QSqlQuery DataBaseHandler::getFoodItem(int foodId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT id, name, price, description FROM foods WHERE id = ?");
    q.addBindValue(foodId);
    q.exec();
    return q;
}

// --- Orders ---
QSqlQuery DataBaseHandler::readAllOrders() {
    QSqlQuery q(m_db);
    if (!q.exec("SELECT * FROM orders ORDER BY id DESC")) {
        qDebug() << "Failed to read all orders:" << q.lastError().text();
    }
    return q;
}
QSqlQuery DataBaseHandler::getOrdersForRestaurant(int restaurantId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT o.id, o.status, o.total_price, o.created_at, u.username "
              "FROM orders o JOIN users u ON o.customer_id = u.id "
              "WHERE o.restaurant_id = ? ORDER BY o.created_at DESC");
    q.addBindValue(restaurantId);
    q.exec();
    return q;
}
QSqlQuery DataBaseHandler::getOrderDetails(int orderId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM orders WHERE id = ?");
    q.addBindValue(orderId);
    q.exec();
    return q;
}
QSqlQuery DataBaseHandler::getOrderItems(int orderId) {
    QSqlQuery q(m_db);
    q.prepare("SELECT mi.name, oi.quantity, oi.price_per_item "
              "FROM order_items oi "
              "JOIN menu_items mi ON oi.menu_item_id = mi.id "
              "WHERE oi.order_id = ?");
    q.addBindValue(orderId);
    if (!q.exec()) {
        qDebug() << "Failed to get order items:" << q.lastError().text();
    }
    return q;
}
bool DataBaseHandler::createNewOrder(const QJsonObject& orderData) {
    QSqlQuery query(m_db);
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
bool DataBaseHandler::addOrderItems(int orderId, const QMap<int, CartItem>& items) {
    if (!m_db.transaction()) {
        qDebug() << "Failed to start transaction for adding order items.";
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO order_items (order_id, menu_item_id, quantity, price_per_item) "
                  "VALUES (:order_id, :menu_item_id, :quantity, :price_per_item)");
    for (const CartItem& item : items) {
        query.bindValue(":order_id", orderId);
        query.bindValue(":menu_item_id", item.foodData["id"].toInt());
        query.bindValue(":quantity", item.quantity);
        query.bindValue(":price_per_item", item.foodData["price"].toDouble());
        if (!query.exec()) {
            qDebug() << "Failed to add order item with menu_id" << item.foodData["id"].toInt() << ":" << query.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}
bool DataBaseHandler::updateOrderStatus(int orderId, const QString& newStatus) {
    QSqlQuery query(m_db);
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
bool DataBaseHandler::markOrderAsReviewed(int orderId) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE orders SET review_submitted = 1 WHERE id = ?");
    q.addBindValue(orderId);
    return q.exec();
}
bool DataBaseHandler::createTestOrder(int restaurantId, int customerId, double totalPrice, const QString& status) {
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

// --- Utilities ---
void DataBaseHandler::logDatabaseStatus() {
    qDebug() << "Database status:";
    qDebug() << "- Open:" << m_db.isOpen();
    qDebug() << "- Tables:" << m_db.tables();
    qDebug() << "- Last error:" << m_db.lastError().text();

    QSqlQuery q("PRAGMA foreign_keys", m_db);
    if (q.exec() && q.next()) {
        qDebug() << "- Foreign keys enabled:" << q.value(0).toBool();
    }
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
