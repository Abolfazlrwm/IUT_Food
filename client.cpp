#include "client.h"
#include "ui_client.h"
#include "profilepanel.h"
#include "shoppingcartpopup.h"
#include "checkoutdialog.h" // برای استفاده از دیالوگ پرداخت
#include "databasehandler.h"
#include <QPropertyAnimation>
#include <QRect>
#include "restaurantitemwidget.h"
#include "databasehandler.h"
#include "datatypes.h"
#include <QListWidgetItem>
#include <QSqlQuery>
#include <QVariant>
#include <QLabel>
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QToolButton>
#include <QWidgetAction>
#include <QEvent>
#include <QMouseEvent>

Client::Client(DataBaseHandler *dbHandler ,QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Client)
    , m_dbHandler(dbHandler)
{
    ui->setupUi(this);

    m_profilePanel = nullptr;
    m_cartMenu = nullptr;
    m_cartPopup = nullptr;
    connect(ui->applyFilterButton, &QPushButton::clicked, this, &Client::on_applyFilterButton_clicked);
    on_applyFilterButton_clicked();



    installEventFilter(this);
    setupToolbarActions();
}

Client::~Client()
{
    delete ui;
}

void Client::setupToolbarActions()
{
    // اتصال دکمه پروفایل
    connect(ui->actionProfile, &QAction::triggered, this, &Client::on_actionProfile_triggered);

    // راه‌اندازی منوی سبد خرید
    createCartMenu();
}

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

// client.cpp

// client.cpp

// client.cpp

void Client::on_actionProfile_triggered()
{
    // اگر پنجره پروفایل از قبل باز است، فقط آن را فعال کرده و به جلو بیاور
    if (m_profilePanel) {
        m_profilePanel->activateWindow();
        m_profilePanel->raise();
        return;
    }

    // ۱. یک نمونه جدید بدون والد می‌سازیم تا یک پنجره مستقل باشد
    m_profilePanel = new ProfilePanel(m_dbHandler ,nullptr);

    // ۲. این ویژگی باعث می‌شود با بسته شدن پنجره، حافظه آن به صورت خودکار آزاد شود
    m_profilePanel->setAttribute(Qt::WA_DeleteOnClose);

    // ۳. (اختیاری اما پیشنهادی) این ویژگی ظاهر پنجره را کمی شبیه‌تر به دیالوگ می‌کند
    // (مثلا دکمه‌های Minimize و Maximize را حذف می‌کند)
    m_profilePanel->setWindowFlags(Qt::Dialog);

    // ۴. یک عنوان برای پنجره جدید تنظیم می‌کنیم
    m_profilePanel->setWindowTitle("پروفایل کاربری");

    // ۵. وقتی پنجره بسته شد، پوینتر را دوباره نال می‌کنیم تا بتوانیم دوباره آن را باز کنیم
    connect(m_profilePanel, &QWidget::destroyed, [this]() {
        m_profilePanel = nullptr;
        qDebug() << "Profile panel window closed and destroyed.";
    });

    // ۶. پنجره جدید را نمایش می‌دهیم
    m_profilePanel->show();
}
// امضای تابع تغییر می‌کند تا کوئری را به عنوان ورودی دریافت کند
void Client::populateRestaurantList(QSqlQuery &query)
{
    ui->restaurantListWidget->clear();

    // دیگر نیازی به اجرای کوئری در اینجا نیست، چون از ورودی تابع می‌آید

    while (query.next())
    {
        Restaurant r;
        r.id = query.value("id").toInt();
        r.name = query.value("name").toString();
        r.type = query.value("type").toString();
        r.location = query.value("location").toString();
        r.priceRange = query.value("price_range").toInt();
        r.logoPath = query.value("logo_path").toString();

        QListWidgetItem *item = new QListWidgetItem(ui->restaurantListWidget);
        RestaurantItemWidget *widget = new RestaurantItemWidget();
        widget->setRestaurantData(r);

        item->setSizeHint(widget->sizeHint());
        ui->restaurantListWidget->setItemWidget(item, widget);
    }
}
bool Client::eventFilter(QObject *watched, QEvent *event)
{
    if (m_profilePanel && event->type() == QEvent::MouseButtonPress) {
        if (!m_profilePanel->geometry().contains(static_cast<QMouseEvent*>(event)->pos())) {
            on_actionProfile_triggered();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void Client::onCartMenuAboutToShow()
{
    m_cartPopup->updateContent();
}

void Client::onShowCheckoutDialog()
{
    if (m_cartMenu && m_cartMenu->isVisible()) {
        m_cartMenu->hide();
    }

    CheckoutDialog checkoutDialog(this);
    checkoutDialog.exec();
}
void Client::on_applyFilterButton_clicked()
{
    if (!m_dbHandler) return;

    // خواندن مقادیر فعلی از تمام فیلترها
    QString nameFilter = ui->searchLineEdit->text();
    QString typeFilter = ui->typeFilterCombo->currentText();
    QString locationFilter = ui->locationFilterCombo->currentText();

    // فراخوانی تابع دیتابیس با همه فیلترها
    QSqlQuery query = m_dbHandler->getAllRestaurants(typeFilter, locationFilter, nameFilter);

    // پر کردن لیست با نتایج فیلتر شده (تابع populateRestaurantList از قبل نوشته شده)
    populateRestaurantList(query);
}
