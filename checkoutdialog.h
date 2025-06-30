#ifndef CHECKOUTDIALOG_H
#define CHECKOUTDIALOG_H
#include "shoppingcart.h"
#include <QDialog>

namespace Ui {
class CheckoutDialog;
}

class CheckoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CheckoutDialog(ShoppingCart* cart, QWidget *parent = nullptr);
    ~CheckoutDialog();


private:
    Ui::CheckoutDialog *ui;
};

#endif // CHECKOUTDIALOG_H
