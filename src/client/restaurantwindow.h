#ifndef RESTAURANTWINDOW_H
#define RESTAURANTWINDOW_H

#include <QMainWindow>

class QLineEdit;
class QComboBox;
class QPushButton;
class QTableWidget;
class QTextEdit;
class Restaurant;

//! Dashboard for a signed-in restaurant: profile, menu management,
//! incoming orders and the live customer chat.
class RestaurantWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit RestaurantWindow(Restaurant *restaurant, QWidget *parent = nullptr);
    ~RestaurantWindow() override;

private slots:
    void on_saveProfileButton_clicked();
    void on_addFoodButton_clicked();
    void on_editFoodButton_clicked();
    void on_deleteFoodButton_clicked();
    void on_confirmOrderButton_clicked();
    void on_shipOrderButton_clicked();

private:
    void setupUi();
    void setupConnections();
    void updateProfileView();
    void updateMenuView();
    void updateOrdersView();

    Restaurant *m_restaurant;

    // profile tab
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_typeEdit = nullptr;
    QLineEdit *m_locationEdit = nullptr;
    QComboBox *m_priceRangeCombo = nullptr;
    QPushButton *m_saveProfileButton = nullptr;

    // menu tab
    QLineEdit *m_foodNameEdit = nullptr;
    QLineEdit *m_foodPriceEdit = nullptr;
    class QTextEdit *m_foodDescEdit = nullptr;
    QPushButton *m_addFoodButton = nullptr;
    QPushButton *m_editFoodButton = nullptr;
    QPushButton *m_deleteFoodButton = nullptr;
    QTableWidget *m_menuTable = nullptr;

    // orders tab
    QTableWidget *m_ordersTable = nullptr;
    QPushButton *m_confirmOrderButton = nullptr;
    QPushButton *m_shipOrderButton = nullptr;

    // chat panel
    QTextEdit *m_chatHistory = nullptr;
    QLineEdit *m_chatInput = nullptr;
    QPushButton *m_chatSendButton = nullptr;
};

#endif // RESTAURANTWINDOW_H
