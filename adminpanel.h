#ifndef ADMINPANEL_H
#define ADMINPANEL_H

#include <QMainWindow>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QMessageBox>
#include <QHeaderView>
#include <QDebug>
#include <QObject>



#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QChart>
#include <QValueAxis>
#include <databasehandler.h>

class AdminPanel : public QMainWindow
{
    Q_OBJECT


public:
    explicit AdminPanel(DataBaseHandler* handler,QWidget *parent = nullptr);
    ~AdminPanel();


private slots:
    // User Management
    void loadUsers();
    void blockSelectedUser();
    void unblockSelectedUser();
    void deleteSelectedUser();
    void approveSelectedRestaurant();
    void disapproveSelectedRestaurant();

    // Orders
    void loadOrders();

    // Charts
    void generateChart();
    void showFoodSalesChart();
    void showRestaurantRevenueChart();
    void showActiveRestaurantsChart();


private:
    // Shared
    DataBaseHandler* dbHandler;

    // Tabs
    QTabWidget* tabWidget;

    // User Management Tab
    QWidget* userTab;
    QTableWidget* tableUsers;
    QPushButton* btnBlock;
    QPushButton* btnUnblock;
    QPushButton* btnDelete;
    QPushButton* btnApprove;
    QPushButton* btnDisapprove;
    QPushButton* btnRefreshUsers;

    // Orders Tab
    QWidget* ordersTab;
    QTableWidget* tableOrders;
    QPushButton* btnRefreshOrders;

    // Reports Tab
    QWidget* reportsTab;
    QComboBox* comboChartType;
    QPushButton* btnGenerateChart;
    QChartView* chartView;

    // Helpers
    int getSelectedUserId() const;
    void setupConnections();

    // UI Setup
    void setupUI();
    void setupUserTab();
    void setupOrdersTab();
    void setupReportsTab();
};

#endif // ADMINPANEL_H
