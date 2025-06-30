#ifndef MENUDIALOG_H
#define MENUDIALOG_H

#include <QDialog>
#include <QJsonObject>

class DataBaseHandler;

namespace Ui { class MenuDialog; }

class MenuDialog : public QDialog
{
    Q_OBJECT
public:
    // کانستراکتور حالا نام رستوران را هم دریافت می‌کند
    explicit MenuDialog(int restaurantId, const QString& restaurantName, DataBaseHandler *dbHandler, QWidget *parent = nullptr);
    ~MenuDialog();

signals:
    void itemAddedToCart(const QJsonObject& foodData, int quantity);

private:
    void populateMenuItems();

    Ui::MenuDialog *ui;
    DataBaseHandler *m_dbHandler;
    int m_restaurantId;
};
#endif // MENUDIALOG_H
