#include "checkoutdialog.h"
#include "ui_checkoutdialog.h"
#include "databasehandler.h"
#include "shoppingcart.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

CheckoutDialog::CheckoutDialog(ShoppingCart *cart, int userId, DataBaseHandler *dbHandler,
                               QWidget *parent)
    : QDialog(parent), ui(new Ui::CheckoutDialog),
      m_dbHandler(dbHandler), m_userId(userId)
{
    ui->setupUi(this);

    // Persian captions for the standard buttons.
    if (auto *okButton = ui->buttonBox->button(QDialogButtonBox::Ok))
        okButton->setText(QStringLiteral("پرداخت و ثبت سفارش"));
    if (auto *cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel))
        cancelButton->setText(QStringLiteral("انصراف"));

    m_baseTotal = cart->getTotalPrice();
    const auto &items = cart->getItems();
    for (const CartItem &cartItem : items) {
        ui->summaryListWidget->addItem(
            QStringLiteral("%1 ×%2 — %3 تومان")
                .arg(cartItem.foodData.value(QLatin1String("name")).toString())
                .arg(cartItem.quantity)
                .arg(cartItem.foodData.value(QLatin1String("price")).toDouble()
                     * cartItem.quantity));
    }

    connect(ui->applyCouponButton, &QPushButton::clicked,
            this, &CheckoutDialog::onApplyCoupon);
    connect(ui->paymentCombo, &QComboBox::currentIndexChanged,
            this, &CheckoutDialog::refreshTotals);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refreshTotals();
}

CheckoutDialog::~CheckoutDialog()
{
    delete ui;
}

QString CheckoutDialog::paymentMethod() const
{
    return (ui->paymentCombo->currentIndex() == 1)
               ? QStringLiteral("wallet") : QStringLiteral("cash");
}

double CheckoutDialog::finalTotal() const
{
    return m_baseTotal - m_discount;
}

void CheckoutDialog::onApplyCoupon()
{
    if (!m_dbHandler)
        return;

    const QString code = ui->couponEdit->text().trimmed();
    if (code.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("کد تخفیف"),
                                 QStringLiteral("ابتدا یک کد تخفیف وارد کنید."));
        return;
    }

    const CouponResult result = m_dbHandler->validateCoupon(code, m_baseTotal);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("کد تخفیف"), result.message);
        return;
    }

    m_discount = result.discount;
    m_couponId = result.couponId;
    m_couponCode = code.toUpper();
    ui->couponEdit->setEnabled(false);
    ui->applyCouponButton->setEnabled(false);
    refreshTotals();

    QMessageBox::information(this, QStringLiteral("کد تخفیف"), result.message);
}

void CheckoutDialog::refreshTotals()
{
    const bool wallet = (paymentMethod() == QLatin1String("wallet"));

    if (m_discount > 0)
        ui->discountLabel->setText(
            QStringLiteral("🎟 تخفیف (%1): %2 تومان")
                .arg(m_couponCode)
                .arg(QString::number(m_discount, 'f', 0)));
    ui->discountLabel->setVisible(m_discount > 0);

    double balance = 0.0;
    if (wallet && m_dbHandler)
        balance = m_dbHandler->getWalletBalance(m_userId);
    ui->walletBalanceLabel->setText(
        QStringLiteral("موجودی کیف پول: %1 تومان").arg(QString::number(balance, 'f', 0)));
    ui->walletBalanceLabel->setVisible(wallet);

    ui->finalPriceLabel->setText(
        QStringLiteral("💰 مبلغ قابل پرداخت: %1 تومان")
            .arg(QString::number(finalTotal(), 'f', 0)));

    // Block the purchase when the wallet cannot cover the total.
    const bool walletShort = wallet && (balance < finalTotal());
    if (auto *okButton = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setEnabled(!walletShort);
        okButton->setToolTip(walletShort
            ? QStringLiteral("موجودی کیف پول کافی نیست — ابتدا کیف پول را شارژ کنید.")
            : QString());
    }
    if (walletShort) {
        ui->walletBalanceLabel->setStyleSheet(
            QStringLiteral("color: #d93025; font-size: 12px; font-weight: bold;"));
        ui->walletBalanceLabel->setText(
            QStringLiteral("⚠️ موجودی کافی نیست — لطفاً از پروفایل، کیف پول را شارژ کنید."));
    } else {
        ui->walletBalanceLabel->setStyleSheet(
            QStringLiteral("color: %1; font-size: 12px;")
                .arg(wallet ? QStringLiteral("#1e9e5b") : QStringLiteral("#888888")));
    }
}
