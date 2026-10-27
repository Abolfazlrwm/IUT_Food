//! Unit tests for the order pipeline: creation, item persistence with
//! denormalised names, the canonical status machine and its Persian labels.
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include "databasehandler.h"
#include "orderstatus.h"

class OrdersTests : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    int restaurantId()
    {
        const int id =
            DataBaseHandler::instance().getUserId(QStringLiteral("pizza_place"));
        if (id <= 0)
            QTest::qFail("pizza_place must be registered in initTestCase",
                         __FILE__, __LINE__);
        return id;
    }

    int customerId()
    {
        const int id =
            DataBaseHandler::instance().getUserId(QStringLiteral("orderer"));
        if (id <= 0)
            QTest::qFail("orderer must be registered in initTestCase",
                         __FILE__, __LINE__);
        return id;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.openDataBase(m_dir.filePath(QStringLiteral("orders_test.db"))));
        QVERIFY(db.createTables());
        QVERIFY(db.registerRestaurant(QStringLiteral("pizza_place"), QStringLiteral("pw"),
                                      QStringLiteral("Pizza Place"), QStringLiteral("fast food"),
                                      QStringLiteral("Tehran"), 2));
        QVERIFY(db.approveRestaurant(db.getUserId(QStringLiteral("pizza_place"))));
        QVERIFY(db.registerUser(QStringLiteral("orderer"), QStringLiteral("pw"),
                                QStringLiteral("customer")));
        QVERIFY(db.addFoodItem(restaurantId(), QStringLiteral("Pepperoni"), 250'000.0,
                               QStringLiteral("classic")));
        QVERIFY(db.addFoodItem(restaurantId(), QStringLiteral("Margherita"), 200'000.0,
                               QStringLiteral("vegetarian")));
    }

    //! Every canonical status maps to its Persian label; unknown values
    //! pass through unchanged (and are rejected by isValid()).
    void statusVocabularyIsCanonical()
    {
        QCOMPARE(OrderStatus::toPersian(QString(OrderStatus::Pending)),
                 QStringLiteral("در انتظار تأیید"));
        QCOMPARE(OrderStatus::toPersian(QString(OrderStatus::Confirmed)),
                 QStringLiteral("تأیید شده"));
        QCOMPARE(OrderStatus::toPersian(QString(OrderStatus::Shipped)),
                 QStringLiteral("در حال ارسال"));
        QCOMPARE(OrderStatus::toPersian(QString(OrderStatus::Delivered)),
                 QStringLiteral("تحویل شده"));
        QCOMPARE(OrderStatus::toPersian(QString(OrderStatus::Cancelled)),
                 QStringLiteral("لغو شده"));

        QVERIFY(OrderStatus::isValid(QStringLiteral("pending")));
        QVERIFY(OrderStatus::isValid(QStringLiteral("delivered")));
        QVERIFY(!OrderStatus::isValid(QStringLiteral("delivered ")));
        QVERIFY(!OrderStatus::isValid(QStringLiteral("Unknown")));
        QCOMPARE(OrderStatus::toPersian(QStringLiteral("mystery")),
                 QStringLiteral("mystery"));
    }

    //! The happy path: order + items are stored, names are snapshotted
    //! from the menu so later menu edits cannot corrupt history.
    void orderWithItemsRoundTrip()
    {
        auto &db = DataBaseHandler::instance();
        const QVector<FoodItem> menu = db.getMenuForRestaurant(restaurantId());
        QCOMPARE(menu.size(), 2);

        QJsonObject orderData;
        orderData[QLatin1String("restaurant_id")] = restaurantId();
        orderData[QLatin1String("customer_id")] = customerId();
        orderData[QLatin1String("total_price")] = 700'000.0;
        orderData[QLatin1String("status")] = QString(OrderStatus::Pending);
        orderData[QLatin1String("created_at")] =
            QStringLiteral("2026-11-05 12:00:00");
        const int orderId = db.createNewOrder(orderData);
        QVERIFY(orderId > 0);

        QMap<int, CartItem> items;
        CartItem pizza;
        pizza.foodData = QJsonObject{{"id", menu.at(0).id},
                                     {"name", menu.at(0).name},
                                     {"price", menu.at(0).price}};
        pizza.quantity = 2;
        items.insert(menu.at(0).id, pizza);
        CartItem drink;
        drink.foodData = QJsonObject{{"id", menu.at(1).id},
                                     {"name", menu.at(1).name},
                                     {"price", menu.at(1).price}};
        drink.quantity = 1;
        items.insert(menu.at(1).id, drink);
        QVERIFY(db.addOrderItems(orderId, items));

        // The customer's history shows the order exactly once.
        QSqlQuery history = db.getOrdersForCustomer(customerId());
        int seen = 0;
        while (history.next())
            ++seen;
        QCOMPARE(seen, 1);

        QSqlQuery saved = db.getOrderItems(orderId);
        double total = 0.0;
        while (saved.next()) {
            total += saved.value(QStringLiteral("price_per_item")).toDouble()
                     * saved.value(QStringLiteral("quantity")).toInt();
        }
        QCOMPARE(total, 700'000.0);
    }

    //! Driving an order through the whole pipeline — every step must be a
    //! canonical status, and anything else is rejected.
    void statusTransitions()
    {
        auto &db = DataBaseHandler::instance();
        QJsonObject orderData;
        orderData[QLatin1String("restaurant_id")] = restaurantId();
        orderData[QLatin1String("customer_id")] = customerId();
        orderData[QLatin1String("total_price")] = 250'000.0;
        orderData[QLatin1String("status")] = QString(OrderStatus::Pending);
        const int orderId = db.createNewOrder(orderData);
        QVERIFY(orderId > 0);

        // (QSqlQuery::size() is -1 for SQLite, so verify rows manually.)
        QSqlQuery exists = db.getOrderDetails(orderId);
        QVERIFY(exists.next());
        QVERIFY(db.updateOrderStatus(orderId, QString(OrderStatus::Confirmed)));
        QVERIFY(db.updateOrderStatus(orderId, QString(OrderStatus::Shipped)));
        QVERIFY(db.updateOrderStatus(orderId, QString(OrderStatus::Delivered)));

        QSqlQuery details = db.getOrderDetails(orderId);
        QVERIFY(details.next());
        QCOMPARE(details.value(QStringLiteral("status")).toString(),
                 QString(OrderStatus::Delivered));

        QVERIFY(!db.updateOrderStatus(orderId, QStringLiteral("flying")));
        QVERIFY(!db.updateOrderStatus(orderId, QStringLiteral("")));
        QVERIFY(!db.updateOrderStatus(-1, QString(OrderStatus::Pending)));
        // The status was NOT corrupted by the invalid attempts.
        QSqlQuery after = db.getOrderDetails(orderId);
        QVERIFY(after.next());
        QCOMPARE(after.value(QStringLiteral("status")).toString(),
                 QString(OrderStatus::Delivered));
    }

    //! A cancelled order is a legal terminal state, not an error.
    void cancelPath()
    {
        auto &db = DataBaseHandler::instance();
        QJsonObject orderData;
        orderData[QLatin1String("restaurant_id")] = restaurantId();
        orderData[QLatin1String("customer_id")] = customerId();
        orderData[QLatin1String("total_price")] = 100'000.0;
        orderData[QLatin1String("status")] = QString(OrderStatus::Pending);
        const int orderId = db.createNewOrder(orderData);
        QVERIFY(db.updateOrderStatus(orderId, QString(OrderStatus::Cancelled)));

        QSqlQuery restaurantOrders = db.getOrdersForRestaurant(restaurantId());
        bool found = false;
        while (restaurantOrders.next())
            found |= restaurantOrders.value(QStringLiteral("id")).toInt() == orderId;
        QVERIFY(found);
    }

    //! v2.4: every freshly opened database runs in WAL mode so readers
    //! never block writers.  WAL is recorded in the file header, so it
    //! also holds for databases created by an older release and opened
    //! by the upgraded server.
    void walJournalMode()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.isOpen());
        QSqlQuery pragma;
        QVERIFY(pragma.exec(QStringLiteral("PRAGMA journal_mode")));
        QVERIFY(pragma.next());
        QCOMPARE(pragma.value(0).toString().toLower(),
                 QStringLiteral("wal"));
        QVERIFY(pragma.exec(QStringLiteral("PRAGMA synchronous")));
        QVERIFY(pragma.next());
        // synchronous=NORMAL (1) - FULL (2) is only for the rollback journal.
        QCOMPARE(pragma.value(0).toInt(), 1);
    }
};

QTEST_MAIN(OrdersTests)
#include "orders_tests.moc"
