#ifndef RESTAURANTLISTDIALOG_H
#define RESTAURANTLISTDIALOG_H
#include <QDialog>
#include <QJsonObject>

class DataBaseHandler;
class RestaurantModel; // اضافه شد

namespace Ui { class RestaurantListDialog; }

class RestaurantListDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RestaurantListDialog(const QString& name, const QString& type, const QString& location,
                                  DataBaseHandler *dbHandler, QWidget *parent = nullptr);
    ~RestaurantListDialog();
signals:
    void itemAddedToCart(const QJsonObject& foodData, int quantity);
private slots:
    void onRestaurantItemClicked(const QModelIndex &index);
private:
    Ui::RestaurantListDialog *ui;
    DataBaseHandler *m_dbHandler;
    RestaurantModel *m_model;
};
#endif // RESTAURANTLISTDIALOG_H
