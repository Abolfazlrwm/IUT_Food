#ifndef CHECKOUTDIALOG_H
#define CHECKOUTDIALOG_H

#include <QDialog>

namespace Ui {
class CheckoutDialog;
}

class CheckoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CheckoutDialog(QWidget *parent = nullptr);
    ~CheckoutDialog();

private:
    Ui::CheckoutDialog *ui;
};

#endif // CHECKOUTDIALOG_H
