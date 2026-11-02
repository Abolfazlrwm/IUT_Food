#include "databasehandler.h"
#include "orderstatus.h"

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QSqlError>
#include <QCoreApplication>
#include <QCryptographicHash>

namespace {

//! Demo-grade password hashing.  A production build would use a salted
//! slow hash (bcrypt/Argon2); SHA-256 here keeps the student project
//! dependency-free while never storing raw passwords on disk.
QString hashPassword(const QString &password)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

//! "همه" / "All" / empty mean "no filtering".
bool wantsFilter(const QString &value)
{
    const QString v = value.trimmed();
    return !v.isEmpty() && v.compare(QStringLiteral("همه"), Qt::CaseInsensitive) != 0
                        && v.compare(QLatin1String("All"), Qt::CaseInsensitive) != 0;
}

} // namespace

DataBaseHandler &DataBaseHandler::instance()
{
    static DataBaseHandler instance;
    return instance;
}

bool DataBaseHandler::openDataBase(const QString &path)
{
    if (m_db.isValid())
        m_db.close();
    else
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));

    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        qWarning() << "Cannot open database" << path << ":" << m_db.lastError().text();
        return false;
    }

    QSqlQuery pragmaQuery(m_db);
    pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    qDebug() << "Database opened:" << path;
    return true;
}

bool DataBaseHandler::isOpen() const
{
    return m_db.isValid() && m_db.isOpen();
}

bool DataBaseHandler::createTables()
{
    QSqlQuery q(m_db);
    const QStringList statements = {
        "CREATE TABLE IF NOT EXISTS users ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " username TEXT UNIQUE NOT NULL,"
        " password TEXT NOT NULL,"
        " name TEXT DEFAULT '',"
        " role TEXT NOT NULL,"
        " is_active INTEGER DEFAULT 1,"
        " is_approved INTEGER DEFAULT 0,"
        " restaurant_type TEXT DEFAULT '',"
        " location TEXT DEFAULT '',"
        " price_range INTEGER DEFAULT 1,"
        " address TEXT DEFAULT '')",

        "CREATE TABLE IF NOT EXISTS restaurants ("
        " id INTEGER PRIMARY KEY,"
        " name TEXT,"
        " type TEXT,"
        " location TEXT,"
        " price_range INTEGER)",

        "CREATE TABLE IF NOT EXISTS foods ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " restaurant_id INTEGER NOT NULL,"
        " name TEXT NOT NULL,"
        " price REAL NOT NULL,"
        " description TEXT DEFAULT '',"
        " FOREIGN KEY(restaurant_id) REFERENCES users(id) ON DELETE CASCADE)",

        "CREATE TABLE IF NOT EXISTS orders ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " restaurant_id INTEGER NOT NULL,"
        " customer_id INTEGER NOT NULL,"
        " total_price REAL DEFAULT 0,"
        " status TEXT NOT NULL DEFAULT 'pending',"
        " created_at TEXT,"
        " review_submitted INTEGER DEFAULT 0,"
        " FOREIGN KEY(restaurant_id) REFERENCES users(id) ON DELETE CASCADE,"
        " FOREIGN KEY(customer_id)  REFERENCES users(id) ON DELETE CASCADE)",

        "CREATE TABLE IF NOT EXISTS order_items ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " order_id INTEGER NOT NULL,"
        " food_id INTEGER,"
        " food_name TEXT,"
        " quantity INTEGER DEFAULT 1,"
        " price_per_item REAL DEFAULT 0,"
        " FOREIGN KEY(order_id) REFERENCES orders(id) ON DELETE CASCADE)",

        "CREATE TABLE IF NOT EXISTS reviews ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " order_id INTEGER NOT NULL UNIQUE,"
        " restaurant_id INTEGER NOT NULL,"
        " customer_id INTEGER NOT NULL,"
        " rating INTEGER NOT NULL CHECK(rating BETWEEN 1 AND 5),"
        " comment TEXT DEFAULT '',"
        " created_at TEXT,"
        " FOREIGN KEY(order_id) REFERENCES orders(id) ON DELETE CASCADE)",

        // Hot lookup paths used by every screen.
        "CREATE INDEX IF NOT EXISTS idx_orders_restaurant ON orders(restaurant_id)",
        "CREATE INDEX IF NOT EXISTS idx_orders_customer   ON orders(customer_id)",
        "CREATE INDEX IF NOT EXISTS idx_foods_restaurant  ON foods(restaurant_id)",
        "CREATE INDEX IF NOT EXISTS idx_reviews_restaurant ON reviews(restaurant_id)",
    };

    for (const QString &statement : statements) {
        if (!q.exec(statement)) {
            qWarning() << "createTables failed:" << q.lastError().text()
                       << "\n statement:" << statement;
            return false;
        }
    }
    return true;
}

// ===========================================================================
// Users
// ===========================================================================

bool DataBaseHandler::registerUser(const QString &userName, const QString &password,
                                   const QString &role)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO users (username, password, role, is_active, is_approved) "
        "VALUES (:u, :p, :r, 1, :a)"));
    q.bindValue(QStringLiteral(":u"), userName);
    q.bindValue(QStringLiteral(":p"), hashPassword(password));
    q.bindValue(QStringLiteral(":r"), role);
    q.bindValue(QStringLiteral(":a"), role == QLatin1String("restaurant") ? 0 : 1);
    if (!q.exec()) {
        qWarning() << "registerUser failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool DataBaseHandler::registerCustomer(const QString &userName, const QString &password,
                                       const QString &name, const QString &address)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO users (username, password, name, role, is_active, is_approved, address) "
        "VALUES (:u, :p, :n, 'customer', 1, 1, :a)"));
    q.bindValue(QStringLiteral(":u"), userName);
    q.bindValue(QStringLiteral(":p"), hashPassword(password));
    q.bindValue(QStringLiteral(":n"), name);
    q.bindValue(QStringLiteral(":a"), address);
    if (!q.exec()) {
        qWarning() << "registerCustomer failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool DataBaseHandler::registerRestaurant(const QString &username, const QString &password,
                                         const QString &name, const QString &type,
                                         const QString &location, int priceRange)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO users (username, password, role, name, is_approved, "
        "restaurant_type, location, price_range) "
        "VALUES (:u, :p, 'restaurant', :n, 0, :t, :l, :pr)"));
    q.bindValue(QStringLiteral(":u"), username);
    q.bindValue(QStringLiteral(":p"), hashPassword(password));
    q.bindValue(QStringLiteral(":n"), name);
    q.bindValue(QStringLiteral(":t"), type);
    q.bindValue(QStringLiteral(":l"), location);
    q.bindValue(QStringLiteral(":pr"), priceRange);
    if (!q.exec()) {
        qWarning() << "registerRestaurant failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QSqlQuery DataBaseHandler::readAllUsers()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT * FROM users ORDER BY id"));
    return q;
}

bool DataBaseHandler::deleteUser(int userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM users WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), userId);
    return q.exec();
}

bool DataBaseHandler::blockUser(int userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE users SET is_active = 0 WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), userId);
    return q.exec();
}

bool DataBaseHandler::unblockUser(int userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE users SET is_active = 1 WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), userId);
    return q.exec();
}

bool DataBaseHandler::approveRestaurant(int userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE users SET is_approved = 1 WHERE id = :id AND role = 'restaurant'"));
    q.bindValue(QStringLiteral(":id"), userId);
    return q.exec();
}

bool DataBaseHandler::disapproveRestaurant(int userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE users SET is_approved = 0 WHERE id = :id AND role = 'restaurant'"));
    q.bindValue(QStringLiteral(":id"), userId);
    return q.exec();
}

bool DataBaseHandler::loginUser(const QString &userName, const QString &password,
                                QString &role, int &userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, role FROM users "
        "WHERE username = :u AND password = :p AND is_active = 1 AND is_approved = 1"));
    q.bindValue(QStringLiteral(":u"), userName);
    q.bindValue(QStringLiteral(":p"), hashPassword(password));

    if (q.exec() && q.next()) {
        userId = q.value(QStringLiteral("id")).toInt();
        role = q.value(QStringLiteral("role")).toString();
        return true;
    }
    return false;
}

QString DataBaseHandler::getUserRole(const QString &username)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT role FROM users WHERE username = :u"));
    q.bindValue(QStringLiteral(":u"), username);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return QString();
}

QString DataBaseHandler::getUserName(int userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT username FROM users WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), userId);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return QString();
}

QSqlQuery DataBaseHandler::getUserDetails(int userId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT username, name, address FROM users WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), userId);
    q.exec();
    return q;
}

bool DataBaseHandler::updateUserDetails(int userId, const QString &newUsername,
                                        const QString &newAddress)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE users SET username = :u, address = :a WHERE id = :id"));
    q.bindValue(QStringLiteral(":u"), newUsername);
    q.bindValue(QStringLiteral(":a"), newAddress);
    q.bindValue(QStringLiteral(":id"), userId);
    if (!q.exec()) {
        qWarning() << "updateUserDetails failed:" << q.lastError().text();
        return false;
    }
    return true;
}

int DataBaseHandler::getUserId(const QString &username)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id FROM users WHERE username = :u"));
    q.bindValue(QStringLiteral(":u"), username);
    return (q.exec() && q.next()) ? q.value(0).toInt() : -1;
}

// ===========================================================================
// Restaurants
// ===========================================================================

//! The authoritative restaurant catalogue lives in `users` (role =
//! 'restaurant').  Querying it directly means restaurants registered on
//! this machine are browsable immediately, even without the TCP server.
QSqlQuery DataBaseHandler::getAllRestaurants(const QString &typeFilter,
                                             const QString &locationFilter,
                                             const QString &nameFilter)
{
    QString sql = QStringLiteral(
        "SELECT id, name, restaurant_type AS type, location, price_range "
        "FROM users WHERE role = 'restaurant' AND is_approved = 1");
    if (wantsFilter(typeFilter))
        sql += QStringLiteral(" AND restaurant_type = :type");
    if (wantsFilter(locationFilter))
        sql += QStringLiteral(" AND location = :loc");
    if (wantsFilter(nameFilter))
        sql += QStringLiteral(" AND name LIKE :name");
    sql += QStringLiteral(" ORDER BY name");

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (wantsFilter(typeFilter))
        q.bindValue(QStringLiteral(":type"), typeFilter.trimmed());
    if (wantsFilter(locationFilter))
        q.bindValue(QStringLiteral(":loc"), locationFilter.trimmed());
    if (wantsFilter(nameFilter))
        q.bindValue(QStringLiteral(":name"), "%" + nameFilter.trimmed() + "%");
    q.exec();
    return q;
}

QSqlQuery DataBaseHandler::readRestaurantDetails(int restaurantId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT name, restaurant_type, location, price_range FROM users WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), restaurantId);
    q.exec();
    return q;
}

bool DataBaseHandler::updateRestaurantDetails(int restaurantId, const QString &name,
                                              const QString &type, const QString &location,
                                              int priceRange)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE users SET name = :n, restaurant_type = :t, location = :l, price_range = :p "
        "WHERE id = :id"));
    q.bindValue(QStringLiteral(":n"), name);
    q.bindValue(QStringLiteral(":t"), type);
    q.bindValue(QStringLiteral(":l"), location);
    q.bindValue(QStringLiteral(":p"), priceRange);
    q.bindValue(QStringLiteral(":id"), restaurantId);
    if (!q.exec()) {
        qWarning() << "updateRestaurantDetails failed:" << q.lastError().text();
        return false;
    }
    return true;
}

int DataBaseHandler::getOrderCountForRestaurant(int restaurantId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM orders WHERE restaurant_id = :id"));
    q.bindValue(QStringLiteral(":id"), restaurantId);
    return (q.exec() && q.next()) ? q.value(0).toInt() : -1;
}

bool DataBaseHandler::validateRestaurant(int restaurantId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id FROM users WHERE id = :id AND role = 'restaurant' AND is_approved = 1"));
    q.bindValue(QStringLiteral(":id"), restaurantId);
    return q.exec() && q.next();
}

// ===========================================================================
// Client-side restaurant cache (filled from the TCP server)
// ===========================================================================

bool DataBaseHandler::clearRestaurantsTable()
{
    QSqlQuery q(m_db);
    return q.exec(QStringLiteral("DELETE FROM restaurants"));
}

bool DataBaseHandler::addRestaurant(const QJsonObject &restaurantData)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO restaurants (id, name, type, location, price_range) "
        "VALUES (:id, :n, :t, :l, :p)"));
    q.bindValue(QStringLiteral(":id"), restaurantData[QLatin1String("id")].toInt());
    q.bindValue(QStringLiteral(":n"), restaurantData[QLatin1String("name")].toString());
    q.bindValue(QStringLiteral(":t"), restaurantData[QLatin1String("type")].toString());
    q.bindValue(QStringLiteral(":l"), restaurantData[QLatin1String("location")].toString());
    q.bindValue(QStringLiteral(":p"), restaurantData[QLatin1String("price_range")].toInt());
    if (!q.exec()) {
        qWarning() << "addRestaurant failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QSqlQuery DataBaseHandler::getCachedRestaurants(const QString &typeFilter,
                                                const QString &locationFilter,
                                                const QString &nameFilter)
{
    QString sql = QStringLiteral("SELECT id, name, type, location, price_range FROM restaurants WHERE 1=1");
    if (wantsFilter(typeFilter))
        sql += QStringLiteral(" AND type = :type");
    if (wantsFilter(locationFilter))
        sql += QStringLiteral(" AND location = :loc");
    if (wantsFilter(nameFilter))
        sql += QStringLiteral(" AND name LIKE :name");

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (wantsFilter(typeFilter))
        q.bindValue(QStringLiteral(":type"), typeFilter.trimmed());
    if (wantsFilter(locationFilter))
        q.bindValue(QStringLiteral(":loc"), locationFilter.trimmed());
    if (wantsFilter(nameFilter))
        q.bindValue(QStringLiteral(":name"), "%" + nameFilter.trimmed() + "%");
    q.exec();
    return q;
}

// ===========================================================================
// Menu (foods)
// ===========================================================================

bool DataBaseHandler::addFoodItem(int restaurantId, const QString &name,
                                  double price, const QString &description)
{
    if (name.isEmpty() || price <= 0 || !validateRestaurant(restaurantId)) {
        qWarning() << "addFoodItem rejected (bad input or unapproved restaurant)";
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO foods (restaurant_id, name, price, description) "
        "VALUES (:r, :n, :p, :d)"));
    q.bindValue(QStringLiteral(":r"), restaurantId);
    q.bindValue(QStringLiteral(":n"), name);
    q.bindValue(QStringLiteral(":p"), price);
    q.bindValue(QStringLiteral(":d"), description);
    if (!q.exec()) {
        qWarning() << "addFoodItem failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool DataBaseHandler::updateFoodItem(int foodId, const QString &name,
                                     double price, const QString &description)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE foods SET name = :n, price = :p, description = :d WHERE id = :id"));
    q.bindValue(QStringLiteral(":n"), name);
    q.bindValue(QStringLiteral(":p"), price);
    q.bindValue(QStringLiteral(":d"), description);
    q.bindValue(QStringLiteral(":id"), foodId);
    return q.exec() && q.numRowsAffected() > 0;
}

bool DataBaseHandler::deleteFoodItem(int foodId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM foods WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), foodId);
    return q.exec() && q.numRowsAffected() > 0;
}

QVector<FoodItem> DataBaseHandler::getMenuForRestaurant(int restaurantId)
{
    QVector<FoodItem> menu;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, name, price, description FROM foods WHERE restaurant_id = :r ORDER BY id"));
    q.bindValue(QStringLiteral(":r"), restaurantId);
    if (q.exec()) {
        while (q.next()) {
            FoodItem item;
            item.id = q.value(QStringLiteral("id")).toInt();
            item.restaurantId = restaurantId;
            item.name = q.value(QStringLiteral("name")).toString();
            item.price = q.value(QStringLiteral("price")).toDouble();
            item.description = q.value(QStringLiteral("description")).toString();
            menu.append(item);
        }
    }
    return menu;
}

// ===========================================================================
// Orders
// ===========================================================================

QSqlQuery DataBaseHandler::readAllOrders()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT * FROM orders ORDER BY id DESC"));
    return q;
}

QSqlQuery DataBaseHandler::getOrdersForRestaurant(int restaurantId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT o.id, o.restaurant_id, o.status, o.total_price, o.created_at, u.username "
        "FROM orders o JOIN users u ON o.customer_id = u.id "
        "WHERE o.restaurant_id = :r ORDER BY o.id DESC"));
    q.bindValue(QStringLiteral(":r"), restaurantId);
    q.exec();
    return q;
}

QSqlQuery DataBaseHandler::getOrdersForCustomer(int customerId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT o.id, o.status, o.total_price, o.created_at, o.review_submitted, "
        "u.name AS restaurant_name "
        "FROM orders o JOIN users u ON o.restaurant_id = u.id "
        "WHERE o.customer_id = :c ORDER BY o.id DESC"));
    q.bindValue(QStringLiteral(":c"), customerId);
    q.exec();
    return q;
}

QSqlQuery DataBaseHandler::getOrderDetails(int orderId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM orders WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), orderId);
    q.exec();
    return q;
}

//! Uses the denormalised food_name snapshot so history stays readable
//! even after a dish is removed from the menu.
QSqlQuery DataBaseHandler::getOrderItems(int orderId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT food_name, quantity, price_per_item "
        "FROM order_items WHERE order_id = :id ORDER BY id"));
    q.bindValue(QStringLiteral(":id"), orderId);
    q.exec();
    return q;
}

int DataBaseHandler::createNewOrder(const QJsonObject &orderData)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO orders (restaurant_id, customer_id, total_price, status, created_at) "
        "VALUES (:r, :c, :t, :s, :a)"));
    q.bindValue(QStringLiteral(":r"), orderData[QLatin1String("restaurant_id")].toInt());
    q.bindValue(QStringLiteral(":c"), orderData[QLatin1String("customer_id")].toInt());
    q.bindValue(QStringLiteral(":t"), orderData[QLatin1String("total_price")].toDouble());
    q.bindValue(QStringLiteral(":s"), orderData[QLatin1String("status")].toString());
    q.bindValue(QStringLiteral(":a"),
                orderData[QLatin1String("created_at")].toString());
    if (!q.exec()) {
        qWarning() << "createNewOrder failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool DataBaseHandler::addOrderItems(int orderId, const QMap<int, CartItem> &items)
{
    if (!m_db.transaction()) {
        qWarning() << "addOrderItems: cannot start transaction";
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO order_items (order_id, food_id, food_name, quantity, price_per_item) "
        "VALUES (:o, :f, :n, :q, :p)"));
    for (const CartItem &item : items) {
        q.bindValue(QStringLiteral(":o"), orderId);
        q.bindValue(QStringLiteral(":f"), item.foodData[QLatin1String("id")].toInt());
        q.bindValue(QStringLiteral(":n"), item.foodData[QLatin1String("name")].toString());
        q.bindValue(QStringLiteral(":q"), item.quantity);
        q.bindValue(QStringLiteral(":p"), item.foodData[QLatin1String("price")].toDouble());
        if (!q.exec()) {
            qWarning() << "addOrderItems failed:" << q.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}

bool DataBaseHandler::updateOrderStatus(int orderId, const QString &newStatus)
{
    if (!OrderStatus::isValid(newStatus)) {
        qWarning() << "updateOrderStatus: unknown status" << newStatus;
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE orders SET status = :s WHERE id = :id"));
    q.bindValue(QStringLiteral(":s"), newStatus);
    q.bindValue(QStringLiteral(":id"), orderId);
    return q.exec();
}

// ===========================================================================
// Reviews
// ===========================================================================

bool DataBaseHandler::submitReview(int orderId, int restaurantId, int customerId,
                                   int rating, const QString &comment)
{
    if (!m_db.transaction())
        return false;

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO reviews (order_id, restaurant_id, customer_id, rating, comment, created_at) "
        "VALUES (:o, :r, :c, :rt, :cm, datetime('now','localtime'))"));
    insert.bindValue(QStringLiteral(":o"), orderId);
    insert.bindValue(QStringLiteral(":r"), restaurantId);
    insert.bindValue(QStringLiteral(":c"), customerId);
    insert.bindValue(QStringLiteral(":rt"), rating);
    insert.bindValue(QStringLiteral(":cm"), comment);
    if (!insert.exec()) {
        qWarning() << "submitReview failed:" << insert.lastError().text();
        m_db.rollback();
        return false;
    }

    QSqlQuery flag(m_db);
    flag.prepare(QStringLiteral(
        "UPDATE orders SET review_submitted = 1 WHERE id = :id"));
    flag.bindValue(QStringLiteral(":id"), orderId);
    if (!flag.exec()) {
        m_db.rollback();
        return false;
    }

    return m_db.commit();
}

QSqlQuery DataBaseHandler::getReviews()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT r.order_id, r.restaurant_id, r.rating, r.comment, r.created_at, "
        "u.username AS customer_name, rest.name AS restaurant_name "
        "FROM reviews r "
        "JOIN users u ON r.customer_id = u.id "
        "JOIN users rest ON r.restaurant_id = rest.id "
        "ORDER BY r.id DESC"));
    return q;
}
