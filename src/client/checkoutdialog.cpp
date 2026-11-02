#include "checkoutdialog.h"
#include "ui_checkoutdialog.h"
#include "shoppingcart.h"

#include <QPushButton>

CheckoutDialog::CheckoutDialog(ShoppingCart *cart, QWidget *parent)
    : QDialog(parent), ui(new Ui::CheckoutDialog)
{
    ui->setupUi(this);

    // Persian captions for the standard buttons.
    if (auto *okButton = ui->buttonBox->button(QDialogButtonBox::Ok))
        okButton->setText(QStringLiteral("پرداخت و ثبت سفارش"));
    if (auto *cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel))
        cancelButton->setText(QStringLiteral("انصراف"));

    const auto &items = cart->getItems();
    for (const CartItem &cartItem : items) {
        ui->summaryListWidget->addItem(
            QStringLiteral("%1 ×%2 — %3 تومان")
                .arg(cartItem.foodData.value(QLatin1String("name")).toString())
                .arg(cartItem.quantity)
                .arg(cartItem.foodData.value(QLatin1String("price")).toDouble()
                     * cartItem.quantity));
    }
    ui->finalPriceLabel->setText(
        QStringLiteral("💰 مبلغ قابل پرداخت: %1 تومان").arg(cart->getTotalPrice()));

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CheckoutDialog::~CheckoutDialog()
{
    delete ui;
}
