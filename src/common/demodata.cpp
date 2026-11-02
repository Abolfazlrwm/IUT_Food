#include "demodata.h"
#include "databasehandler.h"
#include "orderstatus.h"

#include <QSqlQuery>
#include <QDebug>
#include <tuple>

namespace {

bool isFreshDatabase(DataBaseHandler &db)
{
    QSqlQuery q = db.readAllUsers();
    return !q.next(); // no rows => first launch
}

void addFoods(DataBaseHandler &db, int restaurantId,
              std::initializer_list<std::tuple<const char *, double, const char *>> items)
{
    for (const auto &[name, price, description] : items)
        db.addFoodItem(restaurantId, QString::fromUtf8(name), price, QString::fromUtf8(description));
}

} // namespace

void seedDemoData(DataBaseHandler &db)
{
    if (!isFreshDatabase(db))
        return;

    qDebug() << "First launch detected - seeding demo data...";

    // ---- accounts ----------------------------------------------------
    db.registerUser(QStringLiteral("admin"), QStringLiteral("admin123"), QStringLiteral("admin"));

    const int customerId = [&] {
        db.registerCustomer(QStringLiteral("demo_customer"), QStringLiteral("123456"),
                            QStringLiteral("کاربر نمونه"), QStringLiteral("خوابگاه دانشجویی، اتاق ۱۲"));
        return db.getUserId(QStringLiteral("demo_customer"));
    }();

    struct DemoRestaurant { const char *user; const char *name; const char *type; const char *loc; int price; };
    const DemoRestaurant restaurants[] = {
        {"kebab_hafez",  "کباب‌سرای حافظ",   "ایرانی",   "منطقه یک", 2},
        {"pizza_pardis", "پیتزا پاردیس",     "فست فود",  "منطقه 2",  1},
        {"burger_bam",   "برگر بام",         "فست فود",  "منطقه یک", 1},
    };

    int kebabId = -1;
    int pizzaId = -1;
    for (const DemoRestaurant &r : restaurants) {
        db.registerRestaurant(QString::fromUtf8(r.user), QStringLiteral("123456"),
                              QString::fromUtf8(r.name), QString::fromUtf8(r.type),
                              QString::fromUtf8(r.loc), r.price);
        const int id = db.getUserId(QString::fromUtf8(r.user));
        db.approveRestaurant(id); // demo data ships pre-approved
        if (kebabId < 0) kebabId = id; else if (pizzaId < 0) pizzaId = id;
    }

    const int burgerId = db.getUserId(QStringLiteral("burger_bam"));

    // ---- menus ---------------------------------------------------------
    addFoods(db, kebabId, {
        {"چلوکباب کوبیده",     250000, "دو سیخ کباب کوبیده با برنج ایرانی و کره"},
        {"جوجه کباب زعفرانی",  180000, "یک سیخ جوجه با زعفران، برنج و گوجه کبابی"},
        {"خورش قورمه‌سبزی",    150000, "خورش سنتی با گوشت و لوبیا سبز"},
        {"دوغ آبعلی",          25000,  "دوغ سنتی ۵۰۰ میلی‌لیتری"},
    });
    addFoods(db, pizzaId, {
        {"پیتزا پپرونی",       320000, "خمیر ایتالیایی، پپرونی، پنیر موزارلا"},
        {"پیتزا مخصوص پاردیس", 380000, "قارچ، فلفل دلمه، سوسیس و پنیر فراوان"},
        {"سیب‌زمینی سرخ‌کرده",  90000,  "سیب‌زمینی خلالی با سس مخصوص"},
        {"نوشابا",             40000,  "قوطی ۳۳۰ میلی‌لیتری"},
    });
    addFoods(db, burgerId, {
        {"چیزبرگر دوبل",       240000, "دو لته گوشت ۱۰۰ گرمی با پنیر چدار"},
        {"برگر مخصوص بام",     290000, "گوشت، قارچ، پنیر و سس باربیکیو"},
        {"پیتزا سبزیجات",      260000, "سبزیجات تازه و پنیر موزارلا"},
    });

    // ---- a couple of orders so every screen has content -----------------
    if (customerId > 0 && kebabId > 0 && pizzaId > 0) {
        QJsonObject delivered;
        delivered[QLatin1String("restaurant_id")] = kebabId;
        delivered[QLatin1String("customer_id")] = customerId;
        delivered[QLatin1String("status")] = QString(OrderStatus::Delivered);
        delivered[QLatin1String("total_price")] = 475000.0;
        delivered[QLatin1String("created_at")] =
            QDateTime::currentDateTime().addDays(-2).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        const int orderId = db.createNewOrder(delivered);
        if (orderId > 0) {
            QMap<int, CartItem> cart;
            CartItem kabab;  kabab.foodData[QLatin1String("id")] = 1;
            kabab.foodData[QLatin1String("name")] = QStringLiteral("چلوکباب کوبیده");
            kabab.foodData[QLatin1String("price")] = 250000.0;
            kabab.quantity = 1;
            CartItem doogh;  doogh.foodData[QLatin1String("id")] = 4;
            doogh.foodData[QLatin1String("name")] = QStringLiteral("دوغ آبعلی");
            doogh.foodData[QLatin1String("price")] = 25000.0;
            doogh.quantity = 1;
            cart.insert(1, kabab);
            cart.insert(4, doogh);
            db.addOrderItems(orderId, cart);
            db.submitReview(orderId, kebabId, customerId, 5,
                            QStringLiteral("کیفیت غذا عالی بود، بسته‌بندی هم مرتب بود."));
        }

        QJsonObject pending;
        pending[QLatin1String("restaurant_id")] = pizzaId;
        pending[QLatin1String("customer_id")] = customerId;
        pending[QLatin1String("status")] = QString(OrderStatus::Pending);
        pending[QLatin1String("total_price")] = 410000.0;
        pending[QLatin1String("created_at")] =
            QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        const int pizzaOrderId = db.createNewOrder(pending);
        if (pizzaOrderId > 0) {
            QMap<int, CartItem> cart;
            CartItem pizza; pizza.foodData[QLatin1String("id")] = 5;
            pizza.foodData[QLatin1String("name")] = QStringLiteral("پیتزا پپرونی");
            pizza.foodData[QLatin1String("price")] = 320000.0;
            pizza.quantity = 1;
            CartItem fries; fries.foodData[QLatin1String("id")] = 7;
            fries.foodData[QLatin1String("name")] = QStringLiteral("سیب‌زمینی سرخ‌کرده");
            fries.foodData[QLatin1String("price")] = 90000.0;
            fries.quantity = 1;
            cart.insert(5, pizza);
            cart.insert(7, fries);
            db.addOrderItems(pizzaOrderId, cart);
        }
    }

    qDebug() << "Demo data seeded.";
}
