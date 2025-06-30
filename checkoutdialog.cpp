#include "checkoutdialog.h"
#include "ui_checkoutdialog.h"

CheckoutDialog::CheckoutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CheckoutDialog)
{
    ui->setupUi(this);
    // اتصال دکمه‌های استاندارد (OK, Cancel) به اسلات‌های پذیرش و رد دیالوگ
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
CheckoutDialog::~CheckoutDialog()
{
    delete ui;
}
