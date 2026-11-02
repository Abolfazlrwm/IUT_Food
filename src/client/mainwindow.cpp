#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "adminpanel.h"
#include "client.h"
#include "databasehandler.h"
#include "logindialog.h"
#include "restaurant.h"
#include "restaurantwindow.h"

#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
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
