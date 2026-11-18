#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "adminpanel.h"
#include "client.h"
#include "databasehandler.h"
#include "logindialog.h"
#include "restaurant.h"
#include "restaurantwindow.h"
#include "thememanager.h"
#include "version.h"

#include <QAction>
#include <QMenuBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // v2.8: the launcher titles itself like a product, not a debug window.
    setWindowTitle(QStringLiteral("IUT-Food %1 — سامانه سفارش غذای دانشگاه صنعتی اصفهان")
                       .arg(QString::fromLatin1(IUT_FOOD_VERSION)));

    // v2.8: the three entry points explain themselves on hover.
    ui->userButton->setToolTip(QStringLiteral("ورود مشتری: رستوران‌ها، سبد خرید، کیف پول و پیگیری زنده سفارش"));
    ui->restaurantButton->setToolTip(QStringLiteral("ورود رستوران: مدیریت منو و تایید/ارسال سفارش‌ها"));
    ui->adminButton->setToolTip(QStringLiteral("ورود مدیر: تأیید رستوران‌ها، گزارش‌های زنده و خروجی CSV"));

    // Light/dark switch for the launcher window as well.
    QAction *themeAction = menuBar()->addAction(
        ThemeManager::isDark() ? QStringLiteral("☀️ تم روشن") : QStringLiteral("🌙 تم تاریک"));
    connect(themeAction, &QAction::triggered, this, [this, themeAction]() {
        ThemeManager::toggle();
        themeAction->setText(ThemeManager::isDark()
                                 ? QStringLiteral("☀️ تم روشن")
                                 : QStringLiteral("🌙 تم تاریک"));
    });

    // v2.6: the launcher finally says which release you are running (the
    // server had a --version flag from the start; the GUI had nothing).
    // v2.8: reachable with F1 from anywhere in the launcher, and it now
    // names the license and the repository too.
    QAction *aboutAction = menuBar()->addAction(QStringLiteral("درباره"));
    aboutAction->setShortcut(QKeySequence::HelpContents); // F1
    aboutAction->setToolTip(QStringLiteral("درباره IUT-Food (F1)"));
    connect(aboutAction, &QAction::triggered, this, []() {
        QMessageBox::about(nullptr, QStringLiteral("درباره IUT-Food"),
                           QStringLiteral("<b>IUT-Food %1</b>"
                                          "<br>سامانه سفارش غذای دانشگاه"
                                          "<br>کلاینت و سرور Qt/C++17 با پروتکل JSON روی TCP"
                                          "<br>ساخته‌شده با Qt %2"
                                          "<br>مجوز MIT — github.com/Abolfazlrwm/IUT_Food")
                               .arg(QStringLiteral(IUT_FOOD_VERSION),
                                    QLatin1String(qVersion())));
    });

    // v2.8: the standard Ctrl+Q way out, like every desktop product.
    QAction *quitAction = menuBar()->addAction(QStringLiteral("خروج"));
    quitAction->setShortcut(QKeySequence::Quit); // Ctrl+Q
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_userButton_clicked()
{
    LoginDialog dialog(LoginDialog::Mode::Customer, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Reuse the existing window when the same customer is already signed in.
    if (!m_clientWindow || m_clientWindow->userId() != dialog.userId()) {
        if (m_clientWindow)
            m_clientWindow->close();
        m_clientWindow = new Client(dialog.userId(), dialog.username(),
                                    &DataBaseHandler::instance());
        connect(m_clientWindow, &QObject::destroyed, this, [this]() {
            m_clientWindow = nullptr;
        });
    }
    m_clientWindow->show();
    m_clientWindow->raise();
    m_clientWindow->activateWindow();
}

void MainWindow::on_restaurantButton_clicked()
{
    LoginDialog dialog(LoginDialog::Mode::Restaurant, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (!m_restaurantWindow) {
        // One Restaurant session per login; the old code leaked a new
        // object on every click of the launcher button.
        m_restaurant = new Restaurant(dialog.userId(), dialog.username(), this);
        m_restaurantWindow = new RestaurantWindow(m_restaurant);
        connect(m_restaurantWindow, &QObject::destroyed, this, [this]() {
            m_restaurantWindow = nullptr;
            m_restaurant = nullptr;
        });
    }
    m_restaurantWindow->show();
    m_restaurantWindow->raise();
    m_restaurantWindow->activateWindow();
}

void MainWindow::on_adminButton_clicked()
{
    // v2.2: the admin console is no longer a free ride - it asks for the
    // admin credentials first and only then opens with a live session.
    LoginDialog dialog(LoginDialog::Mode::Admin, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (!m_adminWindow) {
        m_adminWindow = new AdminPanel(this);
        connect(m_adminWindow, &QObject::destroyed, this, [this]() {
            m_adminWindow = nullptr;
        });
    }
    m_adminWindow->show();
    m_adminWindow->raise();
    m_adminWindow->activateWindow();
}
