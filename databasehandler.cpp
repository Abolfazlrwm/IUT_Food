#include "databasehandler.h"

DataBaseHandler::DataBaseHandler() {
    if (!QSqlDatabase::contains("main_connection")) {
        db = QSqlDatabase::addDatabase("QSQLITE", "main_connection");
        db.setDatabaseName("data.db");

        if (db.open()) {
            qDebug() << "✅ Database opened: data.db";
            if (createTables()) {
                qDebug() << "✅ Tables created or already exist.";
            } else {
                qDebug() << "⚠️ Error creating tables.";
            }
        } else {
            qDebug() << "❌ Could not open database: " << db.lastError().text();
        }
    } else {
        db = QSqlDatabase::database("main_connection");
        qDebug() << "✅ Reusing existing connection.";
    }
}


bool DataBaseHandler::createTables()
{
    QSqlQuery q(QSqlDatabase::database("main_connection"));

    // Create users table
    bool ok1 = q.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE,"
        "password TEXT,"
        "role TEXT,"
        "is_active INTEGER DEFAULT 1,"
        "is_approved INTEGER DEFAULT 0)"
        );

    // Create orders table
    bool ok2 = q.exec(
        "CREATE TABLE IF NOT EXISTS orders ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "customer_id INTEGER,"
        "restaurant_id INTEGER,"
        "status TEXT,"
        "total_price REAL,"
        "created_at TEXT)"
        );

    if (!ok1 || !ok2) {
        qDebug() << "❌ Failed to create tables.";
        return false;
    }

    // Ensure admin user exists
    QSqlQuery checkAdmin(QSqlDatabase::database("main_connection"));
    checkAdmin.prepare("SELECT COUNT(*) FROM users WHERE role = 'admin'");
    if (checkAdmin.exec() && checkAdmin.next()) {
        if (checkAdmin.value(0).toInt() == 0) {
            QSqlQuery insertAdmin(QSqlDatabase::database("main_connection"));
            insertAdmin.prepare("INSERT INTO users (username, password, role, is_active, is_approved) "
                                "VALUES (?, ?, 'admin', 1, 1)");
            insertAdmin.addBindValue("admin");
            insertAdmin.addBindValue("admin");
            if (insertAdmin.exec()) {
                qDebug() << "✅ Default admin user created: admin/admin";
            } else {
                qDebug() << "❌ Failed to insert admin user!";
            }
        } else {
            qDebug() << "✅ Admin user already exists.";
        }
    }

    // Insert test restaurant if it doesn't exist
    QSqlQuery checkRest(QSqlDatabase::database("main_connection"));
    checkRest.prepare("SELECT COUNT(*) FROM users WHERE username = 'john'");
    if (checkRest.exec() && checkRest.next() && checkRest.value(0).toInt() == 0) {
        QSqlQuery insertRest(QSqlDatabase::database("main_connection"));
        insertRest.prepare("INSERT INTO users (username, password, role, is_active, is_approved) "
                           "VALUES (?, ?, 'restaurant', 1, 0)");
        insertRest.addBindValue("john");
        insertRest.addBindValue("1234");
        if (insertRest.exec())
            qDebug() << "✅ Test restaurant user added.";
    }

    // Insert test customer if it doesn't exist
    QSqlQuery checkCustomer(QSqlDatabase::database("main_connection"));
    checkCustomer.prepare("SELECT COUNT(*) FROM users WHERE username = 'sara'");
    if (checkCustomer.exec() && checkCustomer.next() && checkCustomer.value(0).toInt() == 0) {
        QSqlQuery insertCustomer(QSqlDatabase::database("main_connection"));
        insertCustomer.prepare("INSERT INTO users (username, password, role, is_active, is_approved) "
                               "VALUES (?, ?, 'customer', 1, 1)");
        insertCustomer.addBindValue("sara");
        insertCustomer.addBindValue("abcd");
        if (insertCustomer.exec())
            qDebug() << "✅ Test customer user added.";
    }

    // Insert sample orders only if orders table is empty
    QSqlQuery countOrders(QSqlDatabase::database("main_connection"));
    if (countOrders.exec("SELECT COUNT(*) FROM orders") && countOrders.next() && countOrders.value(0).toInt() == 0) {
        QSqlQuery insertOrder(QSqlDatabase::database("main_connection"));
        insertOrder.prepare("INSERT INTO orders (customer_id, restaurant_id, status, total_price, created_at) "
                            "VALUES (?, ?, ?, ?, ?)");
        insertOrder.addBindValue(3);
        insertOrder.addBindValue(2);
        insertOrder.addBindValue("pending");
        insertOrder.addBindValue(150.0);
        insertOrder.addBindValue("2025-06-24 14:30");
        insertOrder.exec();

        insertOrder.prepare("INSERT INTO orders (customer_id, restaurant_id, status, total_price, created_at) "
                            "VALUES (?, ?, ?, ?, ?)");
        insertOrder.addBindValue(3);
        insertOrder.addBindValue(2);
        insertOrder.addBindValue("delivered");
        insertOrder.addBindValue(200.0);
        insertOrder.addBindValue("2025-06-23 12:00");
        insertOrder.exec();

        qDebug() << "✅ Sample orders inserted.";
    }

    return true;
}

// User CRUD
bool DataBaseHandler::registerUser(const QString& userName, const QString& password, const QString& role) {
    QSqlQuery q(QSqlDatabase::database("main_connection"));
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
    QSqlQuery q(QSqlDatabase::database("main_connection"));
    q.prepare("SELECT * FROM users WHERE username = ?");
    q.addBindValue(userName);
    q.exec();
    return q;
}

QSqlQuery DataBaseHandler::readAllUsers() {
    QSqlQuery q(QSqlDatabase::database("main_connection"));
    q.exec("SELECT * FROM users");
    return q;
}

bool DataBaseHandler::deleteUser(const QString& userName) {
    QSqlQuery q(QSqlDatabase::database("main_connection"));
    q.prepare("DELETE FROM users WHERE username = ?");
    q.addBindValue(userName);
    return q.exec();
}

bool DataBaseHandler::deleteUser(int userId) {
    QSqlQuery q(QSqlDatabase::database("main_connection"));
    q.prepare("DELETE FROM users WHERE id = ?");
    q.addBindValue(userId);
    return q.exec();
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

// Orders
QSqlQuery DataBaseHandler::readAllOrders() {
    QSqlQuery q(QSqlDatabase::database("main_connection"));
    q.exec("SELECT * FROM orders");
    return q;
}
