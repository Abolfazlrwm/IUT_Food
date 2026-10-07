#ifndef RESTAURANTLISTDIALOG_H
#define RESTAURANTLISTDIALOG_H

#include <QDialog>
#include <QJsonObject>

class DataBaseHandler;
class RestaurantModel;
class QCheckBox;

namespace Ui { class RestaurantListDialog; }

//! Search results window.  Shows the restaurants registered on this
//! machine and merges in any restaurant served by the TCP server.
//! The "favorites only" toggle filters the list down to bookmarked ones.
class RestaurantListDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RestaurantListDialog(const QString &name, const QString &type,
                                  const QString &location, DataBaseHandler *dbHandler,
                                  int userId = -1, QWidget *parent = nullptr);
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
    int m_userId;
    QCheckBox *m_favOnlyCheck = nullptr;
};

#endif // RESTAURANTLISTDIALOG_H
