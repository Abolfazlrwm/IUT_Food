#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "adminpanel.h"
#include "client.h"
#include "restaurant.h"
#include "restaurantwindow.h"
#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_3_clicked();

    void on_userButton_clicked();

    void on_adminButton_clicked();

    void on_restaurantButton_clicked();


private:
    AdminPanel* adminPanelWindow = nullptr;    // mainwindow.h
private:
    Client* clientWindow = nullptr;
    RestaurantWindow* restaurantWindow = nullptr;
private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
