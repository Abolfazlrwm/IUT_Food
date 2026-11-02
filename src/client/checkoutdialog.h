#ifndef CHECKOUTDIALOG_H
#define CHECKOUTDIALOG_H

#include <QDialog>

class ShoppingCart;

namespace Ui { class CheckoutDialog; }

//! Modal order summary shown right before the payment is confirmed.
class CheckoutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CheckoutDialog(ShoppingCart *cart, QWidget *parent = nullptr);
    ~CheckoutDialog() override;

private:
    Ui::CheckoutDialog *ui;
};

#endif // CHECKOUTDIALOG_H
