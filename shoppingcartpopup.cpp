#include "shoppingcartpopup.h"
#include "shoppingcart.h"
#include <QVBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVariant>
ShoppingCartPopup::ShoppingCartPopup(QWidget *parent) : QWidget(parent)
{
    // ساخت ظاهر کلی ویجت
    QVBoxLayout *layout = new QVBoxLayout(this);
    m_itemList = new QListWidget(this);
    m_totalPriceLabel = new QLabel("جمع کل: 0 تومان", this);
    m_checkoutButton = new QPushButton("ادامه و پرداخت نهایی", this);

    layout->addWidget(new QLabel("خلاصه سبد خرید:", this));
    layout->addWidget(m_itemList, 1);
    layout->addWidget(m_totalPriceLabel);
    layout->addWidget(m_checkoutButton);

    // اتصال دکمه به سیگنال کلاس
    connect(m_checkoutButton, &QPushButton::clicked, this, &ShoppingCartPopup::checkoutRequested);

    this->setFixedSize(300, 350);
}



void ShoppingCartPopup::updateContent()
{
    // ابتدا لیست را پاک می‌کنیم
    m_itemList->clear();

    // نمونه سبد خرید مرکزی را می‌گیریم
    ShoppingCart* cart = ShoppingCart::getInstance();
    const auto& items = cart->getItems();

    if (items.isEmpty()) {
        m_itemList->addItem("سبد خرید شما خالی است.");
        m_checkoutButton->setEnabled(false); // دکمه پرداخت را غیرفعال کن
    } else {
        m_checkoutButton->setEnabled(true); // دکمه پرداخت را فعال کن
        // حلقه روی تمام آیتم‌های موجود در سبد خرید
        for (const CartItem &cartItem : items) {
            QString name = cartItem.foodData["name"].toString();
            int quantity = cartItem.quantity;

            QString displayText = QString("%1 (تعداد: %2)")
                                      .arg(name)
                                      .arg(quantity);
            m_itemList->addItem(displayText);
        }
    }

    // محاسبه و نمایش قیمت نهایی
    double totalPrice = cart->getTotalPrice();
    m_totalPriceLabel->setText(QString("جمع کل: %1 تومان").arg(totalPrice));
}
