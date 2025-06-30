#ifndef MENUDIALOG_H
#define MENUDIALOG_H

#include <QDialog>

class DataBaseHandler; // Forward declaration

namespace Ui { class MenuDialog; }

class MenuDialog : public QDialog
{
    Q_OBJECT
public:
    // کانستراکتور، ID رستوران را مستقیما دریافت می‌کند
    explicit MenuDialog(int restaurantId, DataBaseHandler *dbHandler, QWidget *parent = nullptr);
    ~MenuDialog();

private:
    void populateMenuItems();

    Ui::MenuDialog *ui;
    DataBaseHandler *m_dbHandler;
    int m_restaurantId;
signals:
    void itemAddedToCart(const QJsonObject& foodData, int quantity);
};
#endif // MENUDIALOG_H
