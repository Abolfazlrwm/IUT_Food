#include "client.h"
#include "ui_client.h"
#include "profilepanel.h"
#include "shoppingcartpopup.h"
#include "checkoutdialog.h" // برای استفاده از دیالوگ پرداخت

// هدرهای فراموش شده برای انیمیشن
#include <QPropertyAnimation>
#include <QRect>

// سایر هدرها
#include <QLabel>
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QToolButton>
#include <QWidgetAction>
#include <QEvent>
#include <QMouseEvent>

Client::Client(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Client)
{
    ui->setupUi(this);

    m_profilePanel = nullptr;
    m_cartMenu = nullptr;
    m_cartPopup = nullptr;

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
    m_profilePanel = new ProfilePanel(nullptr);

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

    //CheckoutDialog checkoutDialog(this);
    //checkoutDialog.exec();
}
