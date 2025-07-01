#ifndef RESTAURANTLISTDIALOG_H
#define RESTAURANTLISTDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QModelIndex>

// Forward declarations
class DataBaseHandler;
class RestaurantModel;

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
    // اسلات جدید برای دریافت داده از شبکه
    void onRestaurantsReceived(const QJsonArray& restaurantsData);

private:
    void populateModelFromDb(); // این تابع حالا فقط مدل را از دیتابیس محلی پر می‌کند

    Ui::RestaurantListDialog *ui;
    DataBaseHandler *m_dbHandler;
    RestaurantModel *m_model;

    // مقادیر فیلترها
    QString m_nameFilter;
    QString m_typeFilter;
    QString m_locationFilter;
};
#endif // RESTAURANTLISTDIALOG_H
