#ifndef MENUDIALOG_H
#define MENUDIALOG_H

#include <QDialog>
#include <QJsonObject>

class DataBaseHandler;

namespace Ui { class MenuDialog; }

//! Shows the menu of one restaurant; each row is a MenuItemWidget that
//! can push dishes into the shared shopping cart.
class MenuDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MenuDialog(int restaurantId, const QString &restaurantName,
                        DataBaseHandler *dbHandler, QWidget *parent = nullptr);
    ~MenuDialog() override;

signals:
    void itemAddedToCart(const QJsonObject &foodData, int quantity);

private:
    void populateMenuItems();

    Ui::MenuDialog *ui;
    DataBaseHandler *m_dbHandler;
    int m_restaurantId;
};

#endif // MENUDIALOG_H
