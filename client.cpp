#include "client.h"
#include "ui_client.h"

// هدرهای مورد نیاز برای تمام قابلیت‌ها
#include "profilepanel.h"
#include "shoppingcartpopup.h"
#include "checkoutdialog.h"
#include "restaurantitemwidget.h"
#include "databasehandler.h"
#include "networkmanager.h"
#include "datatypes.h"
#include <QLabel>
#include <QDebug>
#include <QListWidgetItem>
#include <QSqlQuery>
#include <QVariant>
#include <QMenu>
#include <QToolButton>
#include <QWidgetAction>
#include <QEvent>
#include <QMouseEvent>
#include <QDebug>


Client::Client(DataBaseHandler *dbHandler, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Client)
    , m_dbHandler(dbHandler) // اشاره‌گر دیتابیس را ذخیره می‌کنیم
{
    ui->setupUi(this);

    // پوینترها را در شروع کار حتما نال می‌کنیم
    m_profilePanel = nullptr;
    m_cartMenu = nullptr;
    m_cartPopup = nullptr;

    installEventFilter(this);
    connect(ui->restaurantListWidget, &QListWidget::itemClicked, this, &Client::onRestaurantClicked);
    // راه‌اندازی تمام اتصالات و بخش‌های دیگر
    setupActions();
    createCartMenu();

    // در شروع، یک بار لیست را بر اساس فیلترهای پیش‌فرض (خالی) پر می‌کنیم
    populateRestaurantList();

    // و یک درخواست برای گرفتن داده‌های جدید به سرور می‌فرستیم
    QJsonObject request;
    request["command"] = "get_restaurants";
    NetworkManager::getInstance()->sendJson(request);
// client.cpp -> انتهای کانستراکتور

// کد تست برای بارگذاری مستقیم عکس



}

Client::~Client()
{
    delete ui;
}

// این تابع تمام connect های برنامه را در یک جای تمیز مدیریت می‌کند
void Client::setupActions()
{
    // اتصالات مربوط به UI اصلی
    connect(ui->actionProfile, &QAction::triggered, this, &Client::on_actionProfile_triggered);
    connect(ui->applyFilterButton, &QPushButton::clicked, this, &Client::on_applyFilterButton_clicked);
    connect(ui->restaurantListWidget, &QListWidget::itemClicked, this, &Client::onRestaurantClicked);
    // client.cpp -> setupActions()

    // اتصال به سیگنال‌های مدیر شبکه
    NetworkManager* netManager = NetworkManager::getInstance();
    connect(netManager, &NetworkManager::orderStatusUpdated, this, &Client::onOrderStatusUpdated);

    connect(netManager, &NetworkManager::restaurantsReceived, this, &Client::onRestaurantsReceived);
}

// این تابع مسئول ساخت منوی شناور سبد خرید است
void Client::createCartMenu()
{
    m_cartMenu = new QMenu(this);
    m_cartPopup = new ShoppingCartPopup(this);

    QWidgetAction *widgetAction = new QWidgetAction(this);
    widgetAction->setDefaultWidget(m_cartPopup);
    m_cartMenu->addAction(widgetAction);

    ui->actionCart->setMenu(m_cartMenu);

    QToolButton *cartButton = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionCart));
    if (cartButton) {
        cartButton->setPopupMode(QToolButton::InstantPopup);
    }

    connect(m_cartMenu, &QMenu::aboutToShow, this, &Client::onCartMenuAboutToShow);
    connect(m_cartPopup, &ShoppingCartPopup::checkoutRequested, this, &Client::onShowCheckoutDialog);
}

//========== اسلات‌های مربوط به شبکه و دیتابیس ==========

// وقتی لیست رستوران‌ها از سرور می‌رسد، این تابع اجرا می‌شود
void Client::onRestaurantsReceived(const QJsonArray& restaurantsData)
{
    qDebug() << "Received restaurant list from server. Updating local cache...";
    // ۱. جدول کش محلی را پاک می‌کنیم
    m_dbHandler->clearRestaurantsTable();
    // ۲. داده‌های جدید را در کش محلی ذخیره می‌کنیم
    for (const QJsonValue &value : restaurantsData) {
        m_dbHandler->addRestaurant(value.toObject());
    }
    // ۳. حالا UI را از روی کش آپدیت شده، بازخوانی می‌کنیم
    populateRestaurantList();
}

// این تابع لیست رستوران‌ها را از دیتابیس محلی (کش) می‌خواند و نمایش می‌دهد
void Client::populateRestaurantList()
{
    ui->restaurantListWidget->clear();
    if (!m_dbHandler) return;

    // مقادیر فیلترها را از UI می‌خوانیم
    QString nameFilter = ui->searchLineEdit->text();
    QString typeFilter = ui->typeFilterCombo->currentText();
    QString locationFilter = ui->locationFilterCombo->currentText();

    // کوئری را بر اساس فیلترها از دیتابیس محلی اجرا می‌کنیم
    QSqlQuery query = m_dbHandler->getAllRestaurants(typeFilter, locationFilter, nameFilter);
    while (query.next())
    {
        Restaurant r;
        r.id = query.value("id").toInt();
        r.name = query.value("name").toString();
        r.type = query.value("type").toString();
        r.location = query.value("location").toString();

        QListWidgetItem *item = new QListWidgetItem(ui->restaurantListWidget);
        item->setData(Qt::UserRole, r.id);
        RestaurantItemWidget *widget = new RestaurantItemWidget();
        widget->setRestaurantData(r);
        item->setSizeHint(widget->sizeHint());
        ui->restaurantListWidget->setItemWidget(item, widget);
    }
}

//========== اسلات‌های مربوط به رویدادهای UI ==========

// وقتی دکمه "اعمال فیلتر" کلیک می‌شود، فقط لیست را بازخوانی می‌کنیم
void Client::on_applyFilterButton_clicked()
{
    populateRestaurantList();
}

#include "menudialog.h" // <<< این هدر را اضافه کنید

#include "shoppingcart.h" // <<< این را اضافه کنید

void Client::onRestaurantClicked(QListWidgetItem *item)
{
    int restaurantId = item->data(Qt::UserRole).toInt();
    // ... (کد گرفتن نام رستوران)

    if (restaurantId > 0) {
        MenuDialog menuDialog(restaurantId, m_dbHandler, this);

        // اتصال سیگنال دیالوگ منو به یک لامبدا برای افزودن آیتم به سبد خرید
        connect(&menuDialog, &MenuDialog::itemAddedToCart, this,
                [](const QJsonObject& foodData, int quantity) {

                    // آیتم را به سبد خرید مرکزی اضافه می‌کنیم
                    ShoppingCart::getInstance()->addItem(foodData, quantity);
                });

        menuDialog.exec();
    }
}

void Client::on_actionProfile_triggered()
{
    if (m_profilePanel) {
        m_profilePanel->activateWindow();
        m_profilePanel->raise();
        return;
    }
    m_profilePanel = new ProfilePanel(m_dbHandler, nullptr);
    m_profilePanel->setAttribute(Qt::WA_DeleteOnClose);
    m_profilePanel->setWindowFlags(Qt::Dialog);
    m_profilePanel->setWindowTitle("پروفایل کاربری");
    connect(m_profilePanel, &QWidget::destroyed, [this]() {
        m_profilePanel = nullptr;
    });
    connect(this, &Client::historyChanged, m_profilePanel, &ProfilePanel::refreshHistory);

    m_profilePanel->show();
}

bool Client::eventFilter(QObject *watched, QEvent *event)
{
    // این تابع دیگر برای پنل پروفایل که پنجره جداگانه است، کاربرد زیادی ندارد
    // اما می‌توانید آن را برای کارهای دیگر نگه دارید
    return QMainWindow::eventFilter(watched, event);
}

void Client::onCartMenuAboutToShow()
{
    m_cartPopup->updateContent();
}

#include "shoppingcart.h" // دسترسی به سبد خرید
#include <QMessageBox>   // برای نمایش پیام به کاربر

void Client::onShowCheckoutDialog()
{
    if (m_cartMenu && m_cartMenu->isVisible()) {
        m_cartMenu->hide();
    }

    ShoppingCart* cart = ShoppingCart::getInstance();
    if (cart->getItems().isEmpty()) {
        QMessageBox::warning(this, "سبد خالی", "سبد خرید شما خالی است!");
        return;
    }

    CheckoutDialog checkoutDialog(this);
    // ۱. دیالوگ را نمایش می‌دهیم و منتظر می‌مانیم تا کاربر آن را ببندد
    if (checkoutDialog.exec() == QDialog::Accepted)
    {
        // ۲. اگر کاربر "تایید" را زد، یک درخواست JSON برای سرور می‌سازیم
        QJsonObject orderRequest;
        orderRequest["command"] = "place_order";

        // اطلاعات سبد خرید را به JSON اضافه می‌کنیم
        QJsonArray itemsArray;
        for(const auto& item : cart->getItems()) {
            QJsonObject foodItem;
            foodItem["id"] = item.foodData["id"];
            foodItem["quantity"] = item.quantity;
            itemsArray.append(foodItem);
        }
        orderRequest["items"] = itemsArray;
        orderRequest["total_price"] = cart->getTotalPrice();
        // TODO: شناسه کاربری که لاگین کرده را هم باید اضافه کنید
        // orderRequest["user_id"] = ...;

        // ۳. درخواست را از طریق شبکه به سرور ارسال می‌کنیم
        NetworkManager::getInstance()->sendJson(orderRequest);

        // ۴. به کاربر اطلاع می‌دهیم و سبد خرید را خالی می‌کنیم
        QMessageBox::information(this, "در حال ثبت", "سفارش شما برای سرور ارسال شد. منتظر تایید بمانید.");
        cart->clearCart();
        // پاپ‌آپ سبد خرید را هم آپدیت می‌کنیم تا خالی نشان داده شود
        m_cartPopup->updateContent();
    }
    else
    {
        // اگر کاربر "لغو" را زد
        QMessageBox::information(this, "لغو شد", "ثبت سفارش توسط شما لغو شد.");
    }
}
void Client::onOrderStatusUpdated(const QJsonObject& orderData)
{
    // داده‌های جدید سفارش را در دیتابیس محلی ذخیره یا آپدیت می‌کنیم
    // (باید تابعی برای آپدیت هم در DataBaseHandler بنویسید)
    m_dbHandler->createNewOrder(orderData); // یا updateOrder

    // به همه خبر می‌دهیم که تاریخچه تغییر کرده است
    emit historyChanged();

    QMessageBox::information(this, "به‌روزرسانی سفارش",
                             QString("وضعیت سفارش شماره %1 تغییر کرد.").arg(orderData["id"].toInt()));
}
