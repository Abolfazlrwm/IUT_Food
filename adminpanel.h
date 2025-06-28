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

private slots:
    void on_btnRefreshUsers_clicked();

private:
    Ui::AdminPanel *ui;
    DataBaseHandler* dbHandler;
};

#endif // ADMINPANEL_H
