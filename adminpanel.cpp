#include "adminpanel.h"
#include "ui_adminpanel.h"

AdminPanel::AdminPanel(DataBaseHandler* handler,QWidget *parent)
    : QMainWindow(parent)
    , dbHandler(handler)
    , ui(new Ui::AdminPanel)
{
    ui->setupUi(this);
}

AdminPanel::~AdminPanel()
{
    delete ui;
}

void AdminPanel::on_btnRefreshUsers_clicked()
{
    loadUsers();
}

void AdminPanel::loadUsers()
{
    // Clear the table
    ui->tableUsers->clearContents();
    ui->tableUsers->setRowCount(0);

    // Setting headers
    QStringList headers;
    headers << "ID" << "Username" << "Role" << "is_active" << "is_approved";
    ui->tableUsers->setColumnCount(headers.size());
    ui->tableUsers->setHorizontalHeaderLabels(headers);

    // Get data from DB
    QSqlQuery q = dbHandler->readAllUsers();
    int row = 0;

    // Setting table rows(filling)
    while (q.next()) {
        ui->tableUsers->insertRow(row);
        ui->tableUsers->setItem(row, 0, new QTableWidgetItem(q.value("id").toString()));
        ui->tableUsers->setItem(row, 1, new QTableWidgetItem(q.value("username").toString()));
        ui->tableUsers->setItem(row, 2, new QTableWidgetItem(q.value("role").toString()));
        ui->tableUsers->setItem(row, 3, new QTableWidgetItem(q.value("is_active").toString()));
        ui->tableUsers->setItem(row, 4, new QTableWidgetItem(q.value("is_approved").toString()));
        row++;
    }

    // Better look
    ui->tableUsers->resizeColumnsToContents();
}


void AdminPanel::on_btnRefreshOrders_clicked()
{
    loadOrders();
}

void AdminPanel::loadOrders()
{
    // Clear the table
    ui->tableOrders->clearContents();
    ui->tableOrders->setRowCount(0);

    // Setting headers
    QStringList headers;
    headers << "id" << "customer_id" << "restaurant_id" << "status" << "total_price" << "created_at";
    ui->tableOrders->setColumnCount(headers.size());
    ui->tableOrders->setHorizontalHeaderLabels(headers);

    // Get data from DB
    QSqlQuery q = dbHandler->readAllOrders();
    int row = 0;

    // Setting table rows(filling)
    while (q.next()) {
        ui->tableOrders->insertRow(row);
        ui->tableOrders->setItem(row, 0, new QTableWidgetItem(q.value("id").toString()));
        ui->tableOrders->setItem(row, 1, new QTableWidgetItem(q.value("customer_id").toString()));
        ui->tableOrders->setItem(row, 2, new QTableWidgetItem(q.value("restaurant_id").toString()));
        ui->tableOrders->setItem(row, 3, new QTableWidgetItem(q.value("status").toString()));
        ui->tableOrders->setItem(row, 4, new QTableWidgetItem(q.value("total_price").toString()));
        ui->tableOrders->setItem(row, 5, new QTableWidgetItem(q.value("created_at").toString()));
        row++;
    }

    // Resize columns in order to see texts
    ui->tableOrders->resizeColumnsToContents();
}
