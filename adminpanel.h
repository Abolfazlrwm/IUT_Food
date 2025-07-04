#ifndef ADMINPANEL_H
#define ADMINPANEL_H

#include "adminnetworkmanager.h"
#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QtCharts>
#include <QChartView>
#include <QBarSet>
#include <QBarSeries>
#include <QPieSeries>
#include <QBarCategoryAxis>
#include <QValueAxis>
#include <QSqlQuery>
#include <QHeaderView>
#include <QMessageBox>
#include <QSqlError>
#include <QDebug>

class AdminPanel : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminPanel(QWidget *parent = nullptr);
    ~AdminPanel();

private slots:
    void loadUsers();
    void loadOrders();
    void blockSelectedUser();
    void unblockSelectedUser();
    void deleteSelectedUser();
    void approveSelectedRestaurant();
    void disapproveSelectedRestaurant();
    void generateChart();
    void onServerResponse(const QJsonObject& response);

private:
    void setupUI();
    void setupUserTab();
    void setupOrdersTab();
    void setupReportsTab();
    void setupConnections();
    int getSelectedUserId() const;

    void showFoodSalesChart();
    void showRestaurantRevenueChart();
    void showActiveRestaurantsChart();

    // UI Members
    QTabWidget *tabWidget;
    QWidget *userTab;
    QWidget *ordersTab;
    QWidget *reportsTab;

    QTableWidget *tableUsers;
    QPushButton *btnBlock;
    QPushButton *btnUnblock;
    QPushButton *btnDelete;
    QPushButton *btnApprove;
    QPushButton *btnDisapprove;
    QPushButton *btnRefreshUsers;

    QTableWidget *tableOrders;
    QPushButton *btnRefreshOrders;

    QComboBox *comboChartType;
    QPushButton *btnGenerateChart;
    QChartView *chartView;

    AdminNetworkManager* m_network;

};

#endif // ADMINPANEL_H
