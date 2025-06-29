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
#include <QSqlQuery>
#include <QVariant>
#include "databasehandler.h"
ProfilePanel::ProfilePanel(DataBaseHandler *dbHandler ,QWidget *parent) : QWidget(parent), m_dbHandler(dbHandler)
{
    qDebug() << "PROFILE PANEL CONSTRUCTOR CALLED!";
    this->setAttribute(Qt::WA_DeleteOnClose);
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

    // --- منطق خواندن از دیتابیس ---
    if (!m_dbHandler) {
        historyList->addItem("خطا: اتصال به دیتابیس برقرار نیست.");
        return page;
    }

    QSqlQuery query = m_dbHandler->readAllOrders();

    bool hasOrders = query.first(); // چک می‌کنیم آیا حداقل یک رکورد وجود دارد
    if (hasOrders) {
        query.previous(); // نشانگر را به قبل از اولین رکورد برمی‌گردانیم تا حلقه همه را بخواند
        while (query.next()) {
            int orderId = query.value("id").toInt();
            QString status = query.value("status").toString();
            double price = query.value("total_price").toDouble();

            QString displayText = QString("سفارش شماره %1 - وضعیت: %2 - مبلغ: %3 تومان")
                                      .arg(orderId)
                                      .arg(status)
                                      .arg(price);

            historyList->addItem(displayText);
        }
    } else {
        historyList->addItem("هیچ سفارشی تاکنون ثبت نشده است.");
    }

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
