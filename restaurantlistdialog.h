#ifndef RESTAURANTLISTDIALOG_H
#define RESTAURANTLISTDIALOG_H

#include <QDialog>
#include <QListWidgetItem>
#include <QJsonObject> // برای سیگنال

// Forward declarations
class DataBaseHandler;
class RestaurantItemWidget;

namespace Ui { class RestaurantListDialog; }

class RestaurantListDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RestaurantListDialog(const QString& name, const QString& type, const QString& location,
                                  DataBaseHandler *dbHandler, QWidget *parent = nullptr);
    ~RestaurantListDialog();

signals:
    // <<< این سیگنال جدید اضافه شد >>>
    // این سیگنال خبر را به پنجره اصلی (Client) می‌رساند
    void itemAddedToCart(const QJsonObject& foodData, int quantity);

private slots:
    void onRestaurantItemClicked(QListWidgetItem *item);

private:
    void populateRestaurantList();
    Ui::RestaurantListDialog *ui;
    DataBaseHandler *m_dbHandler;
    QString m_nameFilter;
    QString m_typeFilter;
    QString m_locationFilter;
};

#endif // RESTAURANTLISTDIALOG_H
