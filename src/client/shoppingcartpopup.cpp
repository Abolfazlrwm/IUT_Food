#include "shoppingcartpopup.h"
#include "shoppingcart.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ShoppingCartPopup::ShoppingCartPopup(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    m_itemList = new QListWidget(this);
    m_totalPriceLabel = new QLabel(QStringLiteral("جمع کل: 0 تومان"), this);
    m_checkoutButton = new QPushButton(QStringLiteral("ادامه و پرداخت نهایی"), this);

    layout->addWidget(new QLabel(QStringLiteral("خلاصه سبد خرید:"), this));
    layout->addWidget(m_itemList, 1);
    layout->addWidget(m_totalPriceLabel);
    layout->addWidget(m_checkoutButton);

    connect(m_checkoutButton, &QPushButton::clicked, this, &ShoppingCartPopup::checkoutRequested);

    setFixedSize(320, 360);
    setLayoutDirection(Qt::RightToLeft);
}

void ShoppingCartPopup::updateContent()
{
    m_itemList->clear();

    ShoppingCart *cart = ShoppingCart::getInstance();
    const auto &items = cart->getItems();

    if (items.isEmpty()) {
        m_itemList->addItem(QStringLiteral("سبد خرید شما خالی است."));
        m_checkoutButton->setEnabled(false);
    } else {
        m_checkoutButton->setEnabled(true);
        for (const CartItem &cartItem : items) {
            m_itemList->addItem(QStringLiteral("%1 ×%2 — %3 تومان")
                                    .arg(cartItem.foodData.value(QLatin1String("name")).toString())
                                    .arg(cartItem.quantity)
                                    .arg(cartItem.foodData.value(QLatin1String("price")).toDouble()
                                         * cartItem.quantity));
        }
    }

    m_totalPriceLabel->setText(
        QStringLiteral("جمع کل: %1 تومان").arg(cart->getTotalPrice()));
}
