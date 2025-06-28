#ifndef ADMINPANEL_H
#define ADMINPANEL_H

#include <QMainWindow>
#include <databasehandler.h>

namespace Ui {
class AdminPanel;
}

class AdminPanel : public QMainWindow
{
    Q_OBJECT


public:
    explicit AdminPanel(DataBaseHandler* handler,QWidget *parent = nullptr);
    ~AdminPanel();
    void loadUsers();
    void loadOrders();
    void blockSelectedUser();
    void unblockSelectedUser();
    void deleteSelectedUser();
    void approveSelectedRestaurant();
    void disapproveSelectedRestaurant();

private slots:
    void on_btnRefreshUsers_clicked();
    void on_btnRefreshOrders_clicked(); 
    void on_btnBlock_clicked();
    void on_btnUnblock_clicked();
    void on_btnDelete_clicked();
    void on_btnApprove_clicked();
    void on_btnDisapprove_clicked();

private:
    Ui::AdminPanel *ui;
    DataBaseHandler* dbHandler;

    // Method for getting user id
    int getSelectedUserId() const;

    // A method to set connections
    void setupConnections();
};

#endif // ADMINPANEL_H
