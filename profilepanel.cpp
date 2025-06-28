#include "profilepanel.h"
//#include "databasemanager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>

ProfilePanel::ProfilePanel(QWidget *parent) : QWidget(parent)
{
    qDebug() << "PROFILE PANEL CONSTRUCTOR CALLED!";
    this->setAttribute(Qt::WA_DeleteOnClose); // این خط را اضافه کنید


    this->setStyleSheet(
        "QWidget { background-color: #FFFFFF; border-radius: 8px; }"
        "QListWidget { border: none; background-color: #F0F0F0; }"
        );
    this->setFixedSize(550, 350);
    setupUI();
}

void ProfilePanel::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(1);

    m_navigationList = new QListWidget(this);
    m_navigationList->setFixedWidth(180);
    m_navigationList->addItem("تاریخچه سفارشات");
    m_navigationList->addItem("تنظیمات پروفایل");

    m_contentStack = new QStackedWidget(this);
    m_contentStack->addWidget(createHistoryPage());
    m_contentStack->addWidget(createSettingsPage());

    mainLayout->addWidget(m_contentStack, 1);
    mainLayout->addWidget(m_navigationList);

    connect(m_navigationList, &QListWidget::currentRowChanged,
            m_contentStack, &QStackedWidget::setCurrentIndex);

    m_navigationList->setCurrentRow(0);
}

QWidget* ProfilePanel::createHistoryPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    QListWidget *historyList = new QListWidget(page);
    layout->addWidget(new QLabel("تاریخچه سفارشات شما:", page));
    layout->addWidget(historyList);
    historyList->addItem("سفارش تست - تحویل شده");
    return page;
}

QWidget* ProfilePanel::createSettingsPage()
{
    QWidget *page = new QWidget;
    QFormLayout *layout = new QFormLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->addRow("نام:", new QLineEdit());
    layout->addRow("آدرس:", new QLineEdit());
    layout->addRow(new QPushButton("ذخیره"));
    return page;
}
