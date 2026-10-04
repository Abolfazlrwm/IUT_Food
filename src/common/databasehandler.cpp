#include "databasehandler.h"
#include "orderstatus.h"
#include "passwordhasher.h"

#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QJsonArray>
#include <QSqlError>
#include <QCoreApplication>

namespace {

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
    // Cached statements belong to the connection they were prepared on;
    // a reopen (tests, --db switch) must start from a clean slate.
    m_statements.clear();

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
    // v2.4 performance profile: WAL keeps readers and writers from
    // blocking each other, synchronous=NORMAL trims the fsync count
    // inside WAL mode, and a busy timeout turns rare contention into a
    // short wait instead of a "database is locked" error.
    pragmaQuery.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragmaQuery.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    pragmaQuery.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
    qDebug() << "Database opened:" << path;
    return true;
}

QSqlQuery &DataBaseHandler::statement(const QString &sql)
{
    auto it = m_statements.find(sql);
    if (it == m_statements.end()) {
        it = m_statements.insert(sql, QSqlQuery(m_db));
        it->setForwardOnly(true); // fastest result cursor for SQLite
        it->prepare(sql);
    }
    return it.value();
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

        // ---- schema v3: wallet, coupons, favorites --------------------------
        "CREATE TABLE IF NOT EXISTS wallet_transactions ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " user_id INTEGER NOT NULL,"
        " amount REAL NOT NULL,"
        " kind TEXT NOT NULL,"
        " note TEXT DEFAULT '',"
        " order_id INTEGER,"
        " created_at TEXT,"
        " FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE)",

        "CREATE TABLE IF NOT EXISTS coupons ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " code TEXT UNIQUE NOT NULL,"
        " kind TEXT NOT NULL DEFAULT 'percent',"
        " value REAL NOT NULL,"
        " min_order REAL DEFAULT 0,"
        " max_uses INTEGER DEFAULT 0,"
        " used_count INTEGER DEFAULT 0,"
        " is_active INTEGER DEFAULT 1,"
        " expires_at TEXT)",

        "CREATE TABLE IF NOT EXISTS favorites ("
        " user_id INTEGER NOT NULL,"
        " restaurant_id INTEGER NOT NULL,"
        " created_at TEXT,"
        " PRIMARY KEY(user_id, restaurant_id),"
        " FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE)",

        // Hot lookup paths used by every screen.
        "CREATE INDEX IF NOT EXISTS idx_orders_restaurant ON orders(restaurant_id)",
        "CREATE INDEX IF NOT EXISTS idx_orders_customer   ON orders(customer_id)",
        "CREATE INDEX IF NOT EXISTS idx_foods_restaurant  ON foods(restaurant_id)",
        "CREATE INDEX IF NOT EXISTS idx_reviews_restaurant ON reviews(restaurant_id)",
        "CREATE INDEX IF NOT EXISTS idx_wallet_user ON wallet_transactions(user_id)",
    };

    for (const QString &statement : statements) {
        if (!q.exec(statement)) {
            qWarning() << "createTables failed:" << q.lastError().text()
                       << "\n statement:" << statement;
            return false;
        }
    }

    // Schema v3 migrations: columns added after the first release.  The
    // ALTERs fail harmlessly on a fresh (already v3) database.
    addColumnIfMissing(QStringLiteral("users"),
                       QStringLiteral("balance REAL NOT NULL DEFAULT 0"));
    addColumnIfMissing(QStringLiteral("orders"),
                       QStringLiteral("discount REAL NOT NULL DEFAULT 0"));
    addColumnIfMissing(QStringLiteral("orders"),
                       QStringLiteral("coupon_code TEXT DEFAULT ''"));
    addColumnIfMissing(QStringLiteral("orders"),
                       QStringLiteral("payment_method TEXT NOT NULL DEFAULT 'cash'"));
    return true;
}

bool DataBaseHandler::addColumnIfMissing(const QString &table, const QString &definition)
{
    QSqlQuery pragma(m_db);
    pragma.prepare(QStringLiteral("PRAGMA table_info(%1)").arg(table));
    const QString column = definition.section(QLatin1Char(' '), 0, 0);
    if (pragma.exec()) {
        while (pragma.next()) {
            if (pragma.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
                return true; // already there
        }
    }
    QSqlQuery alter(m_db);
    const bool ok = alter.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2")
                                   .arg(table, definition));
    if (!ok)
        qWarning() << "migration failed for" << table << ":" << alter.lastError().text();
    return ok;
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
    q.bindValue(QStringLiteral(":p"), PasswordHasher::hash(password));
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
    q.bindValue(QStringLiteral(":p"), PasswordHasher::hash(password));
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
    q.bindValue(QStringLiteral(":p"), PasswordHasher::hash(password));
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
    QSqlQuery &q = statement(QStringLiteral(
        "SELECT id, role, password FROM users "
        "WHERE username = :u AND is_active = 1 AND is_approved = 1"));
    q.bindValue(QStringLiteral(":u"), userName);

    if (q.exec() && q.next()) {
        const QString stored = q.value(QStringLiteral("password")).toString();
        bool authenticated = PasswordHasher::verify(password, stored);

        // One-time upgrade: a pre-v2.1 database stores unsalted SHA-256
        // hex digests.  On the first successful legacy login the hash is
        // replaced with a salted PBKDF2 string, so the migration needs no
        // reset and completes per-user, lazily.
        if (!authenticated
            && PasswordHasher::verifyLegacySha256(password, stored)) {
            authenticated = true;
            QSqlQuery upgrade(m_db);
            upgrade.prepare(QStringLiteral(
                "UPDATE users SET password = :p WHERE id = :id"));
            upgrade.bindValue(QStringLiteral(":p"), PasswordHasher::hash(password));
            upgrade.bindValue(QStringLiteral(":id"),
                              q.value(QStringLiteral("id")).toInt());
            if (!upgrade.exec())
                qWarning() << "password re-hash failed:"
                           << upgrade.lastError().text();
        }

        if (!authenticated)
            return false;
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
                                             const QString &nameFilter,
                                             const QVector<int> &onlyIds)
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
    if (!onlyIds.isEmpty()) {
        QStringList placeholders;
        for (int i = 0; i < onlyIds.size(); ++i)
            placeholders << QStringLiteral(":fav%1").arg(i);
        sql += QStringLiteral(" AND id IN (%1)").arg(placeholders.join(QLatin1String(", ")));
    }
    sql += QStringLiteral(" ORDER BY name");

    QSqlQuery &q = statement(sql);
    if (wantsFilter(typeFilter))
        q.bindValue(QStringLiteral(":type"), typeFilter.trimmed());
    if (wantsFilter(locationFilter))
        q.bindValue(QStringLiteral(":loc"), locationFilter.trimmed());
    if (wantsFilter(nameFilter))
        q.bindValue(QStringLiteral(":name"), "%" + nameFilter.trimmed() + "%");
    for (int i = 0; i < onlyIds.size(); ++i)
        q.bindValue(QStringLiteral(":fav%1").arg(i), onlyIds.at(i));
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
                                                const QString &nameFilter,
                                                const QVector<int> &onlyIds)
{
    QString sql = QStringLiteral("SELECT id, name, type, location, price_range FROM restaurants WHERE 1=1");
    if (wantsFilter(typeFilter))
        sql += QStringLiteral(" AND type = :type");
    if (wantsFilter(locationFilter))
        sql += QStringLiteral(" AND location = :loc");
    if (wantsFilter(nameFilter))
        sql += QStringLiteral(" AND name LIKE :name");
    if (!onlyIds.isEmpty()) {
        QStringList placeholders;
        for (int i = 0; i < onlyIds.size(); ++i)
            placeholders << QStringLiteral(":fav%1").arg(i);
        sql += QStringLiteral(" AND id IN (%1)").arg(placeholders.join(QLatin1String(", ")));
    }

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (wantsFilter(typeFilter))
        q.bindValue(QStringLiteral(":type"), typeFilter.trimmed());
    if (wantsFilter(locationFilter))
        q.bindValue(QStringLiteral(":loc"), locationFilter.trimmed());
    if (wantsFilter(nameFilter))
        q.bindValue(QStringLiteral(":name"), "%" + nameFilter.trimmed() + "%");
    for (int i = 0; i < onlyIds.size(); ++i)
        q.bindValue(QStringLiteral(":fav%1").arg(i), onlyIds.at(i));
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
    QSqlQuery &q = statement(QStringLiteral(
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
    QSqlQuery &q = statement(QStringLiteral(
        "SELECT o.id, o.restaurant_id, o.status, o.total_price, o.created_at, u.username "
        "FROM orders o JOIN users u ON o.customer_id = u.id "
        "WHERE o.restaurant_id = :r ORDER BY o.id DESC"));
    q.bindValue(QStringLiteral(":r"), restaurantId);
    q.exec();
    return q;
}

QSqlQuery DataBaseHandler::getOrdersForCustomer(int customerId)
{
    QSqlQuery &q = statement(QStringLiteral(
        "SELECT o.id, o.status, o.total_price, o.created_at, o.review_submitted, "
        "o.discount, o.coupon_code, o.payment_method, "
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
        "SELECT food_id, food_name, quantity, price_per_item "
        "FROM order_items WHERE order_id = :id ORDER BY id"));
    q.bindValue(QStringLiteral(":id"), orderId);
    q.exec();
    return q;
}

int DataBaseHandler::createNewOrder(const QJsonObject &orderData)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO orders (restaurant_id, customer_id, total_price, status, created_at, "
        "discount, coupon_code, payment_method) "
        "VALUES (:r, :c, :t, :s, :a, :d, :cc, :pm)"));
    q.bindValue(QStringLiteral(":r"), orderData[QLatin1String("restaurant_id")].toInt());
    q.bindValue(QStringLiteral(":c"), orderData[QLatin1String("customer_id")].toInt());
    q.bindValue(QStringLiteral(":t"), orderData[QLatin1String("total_price")].toDouble());
    q.bindValue(QStringLiteral(":s"), orderData[QLatin1String("status")].toString());
    q.bindValue(QStringLiteral(":a"),
                orderData[QLatin1String("created_at")].toString());
    q.bindValue(QStringLiteral(":d"), orderData[QLatin1String("discount")].toDouble());
    q.bindValue(QStringLiteral(":cc"), orderData[QLatin1String("coupon_code")].toString());
    const QString pm = orderData[QLatin1String("payment_method")].toString();
    q.bindValue(QStringLiteral(":pm"), pm.isEmpty() ? QStringLiteral("cash") : pm);
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
    // numRowsAffected() also rejects unknown/deleted order ids, so a
    // no-op update is reported as a failure, not silently "succeeding".
    return q.exec() && q.numRowsAffected() > 0;
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

// ===========================================================================
// Wallet (schema v3)
// ===========================================================================

double DataBaseHandler::getWalletBalance(int userId)
{
    QSqlQuery &q = statement(QStringLiteral("SELECT balance FROM users WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), userId);
    return (q.exec() && q.next()) ? q.value(0).toDouble() : 0.0;
}

bool DataBaseHandler::chargeWallet(int userId, double amount, const QString &note)
{
    if (userId <= 0 || amount <= 0)
        return false;
    if (!m_db.transaction())
        return false;

    QSqlQuery upd(m_db);
    upd.prepare(QStringLiteral("UPDATE users SET balance = balance + :a WHERE id = :id"));
    upd.bindValue(QStringLiteral(":a"), amount);
    upd.bindValue(QStringLiteral(":id"), userId);
    if (!upd.exec() || upd.numRowsAffected() != 1) {
        qWarning() << "chargeWallet update failed:" << upd.lastError().text();
        m_db.rollback();
        return false;
    }

    QSqlQuery tx(m_db);
    tx.prepare(QStringLiteral(
        "INSERT INTO wallet_transactions (user_id, amount, kind, note, created_at) "
        "VALUES (:u, :a, 'topup', :n, datetime('now','localtime'))"));
    tx.bindValue(QStringLiteral(":u"), userId);
    tx.bindValue(QStringLiteral(":a"), amount);
    tx.bindValue(QStringLiteral(":n"), note);
    if (!tx.exec()) {
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

bool DataBaseHandler::payFromWallet(int userId, double amount, int orderId)
{
    if (userId <= 0 || amount <= 0)
        return false;
    if (!m_db.transaction())
        return false;

    // The balance guard lives in the WHERE clause, so two concurrent
    // payments can never overdraw the wallet.
    QSqlQuery upd(m_db);
    upd.prepare(QStringLiteral(
        "UPDATE users SET balance = balance - :a WHERE id = :id AND balance >= :a2"));
    upd.bindValue(QStringLiteral(":a"), amount);
    upd.bindValue(QStringLiteral(":a2"), amount);
    upd.bindValue(QStringLiteral(":id"), userId);
    if (!upd.exec() || upd.numRowsAffected() != 1) {
        m_db.rollback(); // insufficient funds (or unknown user)
        return false;
    }

    QSqlQuery tx(m_db);
    tx.prepare(QStringLiteral(
        "INSERT INTO wallet_transactions (user_id, amount, kind, order_id, created_at) "
        "VALUES (:u, :a, 'payment', :o, datetime('now','localtime'))"));
    tx.bindValue(QStringLiteral(":u"), userId);
    tx.bindValue(QStringLiteral(":a"), amount);
    tx.bindValue(QStringLiteral(":o"), orderId);
    if (!tx.exec()) {
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

QSqlQuery DataBaseHandler::getWalletTransactions(int userId)
{
    QSqlQuery &q = statement(QStringLiteral(
        "SELECT amount, kind, note, order_id, created_at FROM wallet_transactions "
        "WHERE user_id = :u ORDER BY id DESC LIMIT 20"));
    q.bindValue(QStringLiteral(":u"), userId);
    q.exec();
    return q;
}

// ===========================================================================
// Coupons (schema v3)
// ===========================================================================

bool DataBaseHandler::createCoupon(const QString &code, const QString &kind,
                                   double value, double minOrder,
                                   const QString &expiresAt, int maxUses)
{
    const QString normalized = code.trimmed().toUpper();
    if (normalized.isEmpty() || value <= 0)
        return false;
    if (kind != QLatin1String("percent") && kind != QLatin1String("amount"))
        return false;
    if (kind == QLatin1String("percent") && value > 100.0)
        return false;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO coupons (code, kind, value, min_order, max_uses, is_active, expires_at) "
        "VALUES (:c, :k, :v, :m, :x, 1, :e)"));
    q.bindValue(QStringLiteral(":c"), normalized);
    q.bindValue(QStringLiteral(":k"), kind);
    q.bindValue(QStringLiteral(":v"), value);
    q.bindValue(QStringLiteral(":m"), minOrder);
    q.bindValue(QStringLiteral(":x"), maxUses);
    q.bindValue(QStringLiteral(":e"), expiresAt.trimmed());
    if (!q.exec()) {
        qWarning() << "createCoupon failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool DataBaseHandler::setCouponActive(int couponId, bool active)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE coupons SET is_active = :a WHERE id = :id"));
    q.bindValue(QStringLiteral(":a"), active ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), couponId);
    return q.exec() && q.numRowsAffected() > 0;
}

bool DataBaseHandler::consumeCoupon(int couponId)
{
    if (couponId <= 0)
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE coupons SET used_count = used_count + 1 "
        "WHERE id = :id AND is_active = 1 "
        "AND (max_uses = 0 OR used_count < max_uses) "
        "AND (expires_at IS NULL OR expires_at = '' OR expires_at >= datetime('now','localtime'))"));
    q.bindValue(QStringLiteral(":id"), couponId);
    return q.exec() && q.numRowsAffected() > 0;
}

int DataBaseHandler::findCouponId(const QString &code)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id FROM coupons WHERE code = :c"));
    q.bindValue(QStringLiteral(":c"), code.trimmed().toUpper());
    return (q.exec() && q.next()) ? q.value(0).toInt() : -1;
}

QSqlQuery DataBaseHandler::getAllCoupons()
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT * FROM coupons ORDER BY id DESC"));
    return q;
}

CouponResult DataBaseHandler::validateCoupon(const QString &code, double orderTotal)
{
    CouponResult result;
    const QString normalized = code.trimmed().toUpper();
    if (normalized.isEmpty()) {
        result.message = QStringLiteral("کد تخفیف را وارد کنید.");
        return result;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM coupons WHERE code = :c"));
    q.bindValue(QStringLiteral(":c"), normalized);
    if (!q.exec() || !q.next()) {
        result.message = QStringLiteral("کد تخفیف یافت نشد.");
        return result;
    }

    result.couponId = q.value(QStringLiteral("id")).toInt();
    const QString kind = q.value(QStringLiteral("kind")).toString();
    const double value = q.value(QStringLiteral("value")).toDouble();
    const double minOrder = q.value(QStringLiteral("min_order")).toDouble();
    const int maxUses = q.value(QStringLiteral("max_uses")).toInt();
    const int usedCount = q.value(QStringLiteral("used_count")).toInt();
    const bool active = q.value(QStringLiteral("is_active")).toInt() == 1;
    const QString expiresAt = q.value(QStringLiteral("expires_at")).toString();

    if (!active) {
        result.message = QStringLiteral("این کد تخفیف غیرفعال است.");
        return result;
    }
    if (!expiresAt.isEmpty() && expiresAt < QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))) {
        result.message = QStringLiteral("این کد تخفیف منقضی شده است.");
        return result;
    }
    if (maxUses > 0 && usedCount >= maxUses) {
        result.message = QStringLiteral("ظرفیت استفاده از این کد تکمیل شده است.");
        return result;
    }
    if (orderTotal < minOrder) {
        result.message = QStringLiteral("حداقل مبلغ سفارش برای این کد %1 تومان است.").arg(minOrder);
        return result;
    }

    result.ok = true;
    result.discount = (kind == QLatin1String("percent"))
                          ? orderTotal * value / 100.0
                          : value;
    result.discount = qMin(result.discount, orderTotal);
    result.message = QStringLiteral("کد تخفیف اعمال شد 🎉");
    return result;
}

// ===========================================================================
// Favorites (schema v3)
// ===========================================================================

bool DataBaseHandler::isFavorite(int userId, int restaurantId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT 1 FROM favorites WHERE user_id = :u AND restaurant_id = :r"));
    q.bindValue(QStringLiteral(":u"), userId);
    q.bindValue(QStringLiteral(":r"), restaurantId);
    return q.exec() && q.next();
}

bool DataBaseHandler::setFavorite(int userId, int restaurantId, bool favorite)
{
    QSqlQuery q(m_db);
    if (favorite) {
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO favorites (user_id, restaurant_id, created_at) "
            "VALUES (:u, :r, datetime('now','localtime'))"));
        q.bindValue(QStringLiteral(":u"), userId);
        q.bindValue(QStringLiteral(":r"), restaurantId);
        return q.exec();
    }
    q.prepare(QStringLiteral(
        "DELETE FROM favorites WHERE user_id = :u AND restaurant_id = :r"));
    q.bindValue(QStringLiteral(":u"), userId);
    q.bindValue(QStringLiteral(":r"), restaurantId);
    return q.exec();
}

QVector<int> DataBaseHandler::getFavoriteRestaurantIds(int userId)
{
    QVector<int> ids;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT restaurant_id FROM favorites WHERE user_id = :u ORDER BY created_at DESC"));
    q.bindValue(QStringLiteral(":u"), userId);
    if (q.exec()) {
        while (q.next())
            ids.append(q.value(0).toInt());
    }
    return ids;
}
