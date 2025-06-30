#include "checkoutdialog.h"
#include "ui_checkoutdialog.h"

CheckoutDialog::CheckoutDialog(ShoppingCart* cart, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CheckoutDialog)
{
    ui->setupUi(this);
    setWindowTitle("تایید نهایی و پرداخت");

    // پر کردن لیست خلاصه سفارش
    const auto& items = cart->getItems();
    for (const CartItem &cartItem : items) {
        QString name = cartItem.foodData["name"].toString();
        int quantity = cartItem.quantity;
        ui->summaryListWidget->addItem(QString("%1 (تعداد: %2)").arg(name).arg(quantity));
    }
    ui->finalPriceLabel->setText(QString("مبلغ قابل پرداخت: %1 تومان").arg(cart->getTotalPrice()));

    // اتصال دکمه‌های تایید و لغو
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
CheckoutDialog::~CheckoutDialog()
{
    delete ui;
}
