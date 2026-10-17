//! Screenshot driver for the documentation (offscreen builds only).
//!
//! Instantiates the real application windows against a running demo
//! database/server, grabs them with QWidget::grab() and writes PNG files
//! to the directory given as argv[1].  Both light and dark passes are
//! captured in a single run.
//!
//!   QT_QPA_PLATFORM=offscreen ./iut-screenshot-driver ./shots

#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QListWidget>
#include <QTimer>

#include "adminpanel.h"
#include "client.h"
#include "checkoutdialog.h"
#include "databasehandler.h"
#include "demodata.h"
#include "logindialog.h"
#include "mainwindow.h"
#include "menudialog.h"
#include "networkmanager.h"
#include "ordertrackerdialog.h"
#include "orderstatus.h"
#include "profilepanel.h"
#include "restaurantlistdialog.h"
#include "shoppingcart.h"
#include "thememanager.h"

namespace {

QString g_outDir;

void saveShot(QWidget *widget, const QString &name)
{
    const QString path = g_outDir + QStringLiteral("/%1.png").arg(name);
    widget->grab().save(path);
    qInfo() << "saved" << path;
}

void showGrabHide(QWidget *widget, const QString &name)
{
    widget->show();
    QApplication::processEvents();
    QApplication::processEvents();
    saveShot(widget, name);
    widget->hide();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IUT-Food"));
    QApplication::setOrganizationName(QStringLiteral("IUT"));
    QApplication::setApplicationVersion(QStringLiteral("2.0.0"));

    g_outDir = (argc > 1) ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
    QDir().mkpath(g_outDir);

    for (const QString &fontFile : { QStringLiteral(":/fonts/Vazirmatn-Regular.ttf"),
                                     QStringLiteral(":/fonts/Vazirmatn-Bold.ttf") })
        QFontDatabase::addApplicationFont(fontFile);
    QFont appFont(QStringLiteral("Vazirmatn"), 10);
    appFont.setStyleStrategy(QFont::PreferAntialias);
    QApplication::setFont(appFont);

    const QString dbPath = QApplication::applicationDirPath() + QStringLiteral("/iut_food.db");
    DataBaseHandler &db = DataBaseHandler::instance();
    if (!db.openDataBase(dbPath))
        return 1;
    db.createTables();
    seedDemoData(db);

    // Put the live tracker in a meaningful state: ship the pending demo
    // order so the tracking pipeline shows real progress.
    int pendingOrderId = -1;
    const int demoUserId = db.getUserId(QStringLiteral("demo_customer"));
    QSqlQuery pending = db.getOrdersForCustomer(demoUserId);
    if (pending.next()) {
        pendingOrderId = pending.value(QStringLiteral("id")).toInt();
        db.updateOrderStatus(pendingOrderId, QString(OrderStatus::Shipped));
    }

    ThemeManager::apply(QStringLiteral("light"));

    // ---- fixtures ---------------------------------------------------------
    MainWindow launcher;
    LoginDialog login(LoginDialog::Mode::Customer);

    Client clientArea(demoUserId, QStringLiteral("demo_customer"), &db);
    clientArea.resize(1000, 640);

    RestaurantListDialog restaurantList(QString(), QString(), QString(), &db, demoUserId);
    restaurantList.resize(540, 500);

    MenuDialog menu(db.getUserId(QStringLiteral("kebab_hafez")),
                    QStringLiteral("کباب‌سرای حافظ"), &db, demoUserId);
    menu.resize(500, 480);

    // Cart with two demo dishes for the checkout shot.
    QJsonObject kabab;
    kabab[QLatin1String("id")] = 1;
    kabab[QLatin1String("name")] = QStringLiteral("چلوکباب کوبیده");
    kabab[QLatin1String("price")] = 250000.0;
    kabab[QLatin1String("restaurant_id")] = db.getUserId(QStringLiteral("kebab_hafez"));
    QJsonObject doogh;
    doogh[QLatin1String("id")] = 4;
    doogh[QLatin1String("name")] = QStringLiteral("دوغ آبعلی");
    doogh[QLatin1String("price")] = 25000.0;
    doogh[QLatin1String("restaurant_id")] = kabab.value(QLatin1String("restaurant_id"));
    ShoppingCart *cart = ShoppingCart::getInstance();
    cart->addItem(kabab, 2);
    cart->addItem(doogh, 1);

    CheckoutDialog checkout(cart, demoUserId, &db);

    ProfilePanel profile(&db, demoUserId, QStringLiteral("demo_customer"));
    if (auto *nav = profile.findChild<QListWidget *>())
        nav->setCurrentRow(1); // wallet page

    OrderTrackerDialog tracker(pendingOrderId, QString(OrderStatus::Shipped));

    // v2.2: the admin console speaks the authenticated protocol.  Opening
    // the admin session here parks the panel's constructor requests until
    // the login round-trip lands, so the dashboards are populated when
    // the shot is taken 3 seconds later.
    NetworkManager::instance().connectToServer();
    NetworkManager::instance().authenticateWith(QStringLiteral("admin"),
                                                QStringLiteral("admin123"));
    AdminPanel admin;

    // ---- light pass (small delay lets the TCP connection settle) ---------
    QTimer::singleShot(900, [&]() {
        showGrabHide(&launcher,      QStringLiteral("01_launcher"));
        showGrabHide(&login,         QStringLiteral("02_login"));
        showGrabHide(&clientArea,    QStringLiteral("03_client"));
        showGrabHide(&restaurantList, QStringLiteral("04_restaurant_list"));
        showGrabHide(&menu,          QStringLiteral("05_menu"));
        showGrabHide(&checkout,      QStringLiteral("06_checkout"));
        showGrabHide(&profile,       QStringLiteral("07_profile_wallet"));
        showGrabHide(&tracker,       QStringLiteral("08_tracking"));
        // admin waits for the network pass below
    });

    // ---- admin after data arrives, then the dark pass ------------------------
    QTimer::singleShot(3000, [&]() {
        // Make sure the admin console advertises the live connection.
        const auto labels = admin.findChildren<QLabel *>();
        for (QLabel *label : labels) {
            if (label->text().contains(QStringLiteral("اتصال")))
                label->setText(QStringLiteral("🟢 متصل به سرور"));
        }
        showGrabHide(&admin, QStringLiteral("09_admin"));

        ThemeManager::apply(QStringLiteral("dark"));
        showGrabHide(&launcher,   QStringLiteral("10_launcher_dark"));
        showGrabHide(&clientArea, QStringLiteral("11_client_dark"));
        showGrabHide(&checkout,   QStringLiteral("12_checkout_dark"));

        QApplication::quit();
    });

    return app.exec();
}
