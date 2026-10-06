#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class Client;
class AdminPanel;
class RestaurantWindow;
class Restaurant;

namespace Ui { class MainWindow; }

//! Entry launcher: routes to the customer area, the restaurant dashboard
//! or the networked admin panel.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_userButton_clicked();
    void on_restaurantButton_clicked();
    void on_adminButton_clicked();

private:
    Ui::MainWindow *ui;

    Client *m_clientWindow = nullptr;
    AdminPanel *m_adminWindow = nullptr;
    RestaurantWindow *m_restaurantWindow = nullptr;
    Restaurant *m_restaurant = nullptr;
};

#endif // MAINWINDOW_H
