#include "shoppingcartpopup.h"
#include <QVBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>

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
    // TODO: این بخش باید با داده‌های واقعی سبد خرید پر شود
    m_itemList->clear();
    m_itemList->addItem("پیتزا پپرونی (1 عدد)");
    m_totalPriceLabel->setText("جمع کل: 280,000 تومان");
}
