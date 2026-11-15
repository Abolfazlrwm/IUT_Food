#ifndef ADMINPANEL_H
#define ADMINPANEL_H

#include <QJsonObject>
#include <QJsonArray>
#include <QMainWindow>

class QTableWidget;
class QPushButton;
class QTabWidget;
class QComboBox;
class QChartView;
class QLabel;

class NetworkManager;

//! Networked administration console.  Every operation goes through the
//! TCP server (users/orders/reviews live in the server database).
class AdminPanel : public QMainWindow
{
    Q_OBJECT
public:
    explicit AdminPanel(QWidget *parent = nullptr);
    ~AdminPanel() override;

private slots:
    void loadUsers();
    void loadOrders();
    void loadReviews();
    void loadCoupons();
    void loadServerStats();
    void addCoupon();
    void toggleSelectedCoupon();
    void blockSelectedUser();
    void unblockSelectedUser();
    void deleteSelectedUser();
    void approveSelectedRestaurant();
    void disapproveSelectedRestaurant();
    void generateChart();
    void onServerResponse(const QJsonObject &response);

private:
    void setupUI();
    void setupUserTab();
    void setupOrdersTab();
    void setupCouponsTab();
    void setupReportsTab();
    void setupServerTab();
    void setupConnections();
    int getSelectedUserId() const;
    int getSelectedCouponId() const;

    void showOrdersByStatusChart();
    void showRestaurantRevenueChart();
    void showActiveRestaurantsChart();
    void showAverageRatingChart();

    QJsonObject m_lastUsers;
    QJsonObject m_lastOrders;
    QJsonObject m_lastReviews;
    QJsonObject m_lastCoupons;

    QTabWidget *tabWidget;
    QWidget *userTab;
    QWidget *ordersTab;
    QWidget *couponsTab;
    QWidget *reportsTab;
    QWidget *serverTab;

    QTableWidget *tableUsers;
    QPushButton *btnBlock;
    QPushButton *btnUnblock;
    QPushButton *btnDelete;
    QPushButton *btnApprove;
    QPushButton *btnDisapprove;
    QPushButton *btnRefreshUsers;

    QTableWidget *tableOrders;
    QPushButton *btnRefreshOrders;

    QTableWidget *tableCoupons;
    QPushButton *btnAddCoupon;
    QPushButton *btnToggleCoupon;
    QPushButton *btnRefreshCoupons;

    QComboBox *comboChartType;
    QPushButton *btnGenerateChart;
    QChartView *chartView;

    // v2.6: live server-health tab (get_server_stats, admin only).
    QLabel *statUptime;
    QLabel *statSessions;
    QLabel *statSessionsTotal;
    QLabel *statConnections;
    QLabel *statTlsConnections;
    QLabel *statRequestsServed;
    QLabel *statRequestsRejected;
    QLabel *statAuthFailures;
    QPushButton *btnRefreshStats;

    QLabel *m_connectionLabel = nullptr;
    NetworkManager *m_network = nullptr;
};

#endif // ADMINPANEL_H
