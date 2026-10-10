#ifndef CHECKOUTDIALOG_H
#define CHECKOUTDIALOG_H

#include <QDialog>

class ShoppingCart;
class DataBaseHandler;

namespace Ui { class CheckoutDialog; }

/*!
 * Modal order summary shown right before the payment is confirmed.
 *
 * v2 additions: coupon codes (validated live against the coupons table)
 * and wallet payment with a visible balance check - the OK button stays
 * disabled while the wallet cannot cover the discounted total.
 */
class CheckoutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CheckoutDialog(ShoppingCart *cart, int userId, DataBaseHandler *dbHandler,
                            QWidget *parent = nullptr);
    ~CheckoutDialog() override;

    QString couponCode() const { return m_couponCode; }
    int couponId() const { return m_couponId; }
    double discount() const { return m_discount; }
    //! "cash" or "wallet"
    QString paymentMethod() const;

private slots:
    void onApplyCoupon();
    void refreshTotals();

private:
    double finalTotal() const;

    Ui::CheckoutDialog *ui;
    DataBaseHandler *m_dbHandler;
    int m_userId;
    double m_baseTotal = 0.0;
    double m_discount = 0.0;
    int m_couponId = -1;
    QString m_couponCode;
};

#endif // CHECKOUTDIALOG_H
