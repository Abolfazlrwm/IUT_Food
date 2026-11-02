#ifndef RESTAURANTLISTDIALOG_H
#define RESTAURANTLISTDIALOG_H

#include <QDialog>
#include <QJsonObject>

class DataBaseHandler;
class RestaurantModel;

namespace Ui { class RestaurantListDialog; }

//! Search results window.  Shows the restaurants registered on this
//! machine and merges in any restaurant served by the TCP server.
class RestaurantListDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RestaurantListDialog(const QString &name, const QString &type,
                                  const QString &location, DataBaseHandler *dbHandler,
                                  QWidget *parent = nullptr);
    ~RestaurantListDialog() override;

signals:
    void itemAddedToCart(const QJsonObject &foodData, int quantity);

private slots:
    void onRestaurantsReceived(const QJsonArray &restaurantsData);
    void onRestaurantItemClicked(const QModelIndex &index);

private:
    void populateModelFromDb();

    Ui::RestaurantListDialog *ui;
    DataBaseHandler *m_dbHandler;
    RestaurantModel *m_model;
    QString m_nameFilter;
    QString m_typeFilter;
    QString m_locationFilter;
};

#endif // RESTAURANTLISTDIALOG_H
