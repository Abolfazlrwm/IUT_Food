#ifndef RESTAURANTWINDOW_H
#define RESTAURANTWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QComboBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include "restaurant.h"

class RestaurantWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RestaurantWindow(Restaurant* restaurant, QWidget* parent = nullptr);
    ~RestaurantWindow();

private slots:
    void on_saveProfileButton_clicked();
    void on_addFoodButton_clicked();
    void on_editFoodButton_clicked();
    void on_deleteFoodButton_clicked();
    void on_confirmOrderButton_clicked();
    void on_shipOrderButton_clicked();
    void showNewOrderNotification(const QString& customerName);

private:
    void setupUI();
    void setupConnections();
    void updateProfileView();
    void updateMenuView();
    void updateOrdersView();

    Restaurant* m_restaurant;

    // UI Elements
    QTableWidget* m_menuTable;
    QTableWidget* m_ordersTable;

    // Profile Tab
    QLineEdit* m_nameEdit;
    QLineEdit* m_typeEdit;
    QLineEdit* m_locationEdit;
    QComboBox* m_priceRangeCombo;
    QPushButton* m_saveProfileButton;

    // Menu Tab
    QLineEdit* m_foodNameEdit;
    QLineEdit* m_foodPriceEdit;
    QTextEdit* m_foodDescEdit;
    QPushButton* m_addFoodButton;
    QPushButton* m_editFoodButton;
    QPushButton* m_deleteFoodButton;

    // Orders Tab
    QPushButton* m_confirmOrderButton;
    QPushButton* m_shipOrderButton;
};

#endif // RESTAURANTWINDOW_H
