#include "profilepanel.h"
#include "databasehandler.h"
#include "networkmanager.h"

// هدرهای لازم برای ویجت‌های استفاده شده
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QFormLayout>
#include <QGroupBox>     // <<< هدر فراموش شده
#include <QMessageBox>   // <<< هدر فراموش شده
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <QJsonObject>
#include <QIcon>


ProfilePanel::ProfilePanel(DataBaseHandler *dbHandler ,QWidget *parent) : QWidget(parent), m_dbHandler(dbHandler)
{
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setFixedSize(550, 450); // کمی ارتفاع را بیشتر کردم
    m_currentOrderId = -1;
    setupUI();
}

void ProfilePanel::refreshHistory()
{
    QWidget* historyPage = m_contentStack->widget(0);
    if (!historyPage || !m_dbHandler) return;
    QListWidget* historyList = historyPage->findChild<QListWidget*>();
    if (!historyList) return;

    historyList->clear();
    QSqlQuery query = m_dbHandler->readAllOrders();
    while (query.next()) {
        int orderId = query.value("id").toInt();
        QString status = query.value("status").toString();
        QString displayText = QString("سفارش شماره %1 - وضعیت: %2").arg(orderId).arg(status);
        QListWidgetItem *item = new QListWidgetItem(displayText, historyList);
        item->setData(Qt::UserRole, orderId);
    }
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
    m_contentStack->addWidget(createOrderDetailPage());

    mainLayout->addWidget(m_contentStack, 1);
    mainLayout->addWidget(m_navigationList);

    connect(m_navigationList, &QListWidget::currentRowChanged, this, [this](int index){
        if (index == 0 || index == 1) {
            m_navigationList->setVisible(true);
            if(index == 0) refreshHistory();
            m_contentStack->setCurrentIndex(index);
        }
    });
    m_navigationList->setCurrentRow(0);
}

QWidget* ProfilePanel::createHistoryPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    QListWidget *historyList = new QListWidget(page);
    layout->addWidget(new QLabel("تاریخچه سفارشات شما:", page));
    layout->addWidget(historyList);
    connect(historyList, &QListWidget::itemClicked, this, &ProfilePanel::onHistoryItemClicked);
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

#include <QSpinBox> // <<< هدر جدید برای اسپین باکس

QWidget* ProfilePanel::createOrderDetailPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);

    // ... (کد ساخت دکمه بازگشت، لیبل‌ها، لیست آیتم‌ها و بخش چت بدون تغییر باقی می‌ماند) ...

    // ========== بخش جدید و ساده شده برای امتیازدهی ==========
    QGroupBox *ratingBox = new QGroupBox("ثبت امتیاز و نظر", page);
    ratingBox->setObjectName("ratingBox");
    ratingBox->setVisible(false); // در ابتدا مخفی باشد

    QFormLayout *ratingLayout = new QFormLayout(ratingBox); // استفاده از FormLayout برای نظم بیشتر

    // ۱. ساخت اسپین باکس برای گرفتن امتیاز عددی
    QSpinBox* ratingSpinBox = new QSpinBox(ratingBox);
    ratingSpinBox->setObjectName("ratingSpinBox");
    ratingSpinBox->setRange(1, 5); // محدوده امتیاز بین ۱ تا ۵
    ratingSpinBox->setSuffix(" امتیاز"); // یک پسوند برای زیبایی

    // ۲. ساخت کادر متن برای نظر
    QTextEdit* reviewTextEdit = new QTextEdit(ratingBox);
    reviewTextEdit->setPlaceholderText("نظر خود را در مورد این سفارش بنویسید...");

    // ۳. ساخت دکمه ثبت نهایی
    QPushButton* submitReviewButton = new QPushButton("ثبت نهایی نظر", ratingBox);

    // اضافه کردن ویجت‌ها به لی‌آوت فرم
    ratingLayout->addRow("امتیاز شما:", ratingSpinBox);
    ratingLayout->addRow("متن نظر:", reviewTextEdit);
    ratingLayout->addRow(submitReviewButton);

    // ===========================================

    // اتصال دکمه ثبت نظر به منطق ارسال به سرور
    connect(submitReviewButton, &QPushButton::clicked, this, [this, ratingSpinBox, reviewTextEdit, ratingBox](){
        int rating = ratingSpinBox->value(); // خواندن امتیاز از اسپین باکس

        QJsonObject reviewRequest;
        reviewRequest["command"] = "submit_review";
        reviewRequest["order_id"] = m_currentOrderId;
        reviewRequest["rating"] = rating; // ارسال امتیاز عددی
        reviewRequest["review_text"] = reviewTextEdit->toPlainText();

        NetworkManager::getInstance()->sendJson(reviewRequest);

        ratingBox->setEnabled(false);
        ratingBox->setTitle("نظر شما با موفقیت ثبت شد");
        QMessageBox::information(this, "متشکریم", "نظر شما با موفقیت ثبت شد.");
    });

    // اضافه کردن بخش امتیازدهی به لی‌آوت اصلی صفحه
    mainLayout->addWidget(ratingBox);

    return page;
}

// =================================================================
// <<< این توابع حالا در سطح کلاس و خارج از توابع دیگر قرار دارند >>>
// =================================================================

void ProfilePanel::loadOrderDetails(int orderId)
{
    m_currentOrderId = orderId;
    QWidget* detailPage = m_contentStack->widget(2);
    if (!detailPage || !m_dbHandler) return;

    // ... (کد پر کردن اطلاعات سفارش و آیتم‌ها) ...

    QString status;
    QSqlQuery orderQuery = m_dbHandler->getOrderDetails(orderId);
    if (orderQuery.first()) {
        status = orderQuery.value("status").toString();
        // ... (تنظیم لیبل‌ها) ...
    }

    // پیدا کردن جعبه امتیازدهی و کنترل نمایش آن
    QGroupBox* ratingBox = detailPage->findChild<QGroupBox*>("ratingBox");
    if (ratingBox) {
        bool isDelivered = (status == "تحویل شده");
        ratingBox->setVisible(isDelivered);
        ratingBox->setEnabled(isDelivered);
        if(isDelivered) ratingBox->setTitle("ثبت امتیاز و نظر");
    }
}

void ProfilePanel::onHistoryItemClicked(QListWidgetItem *item)
{
    int orderId = item->data(Qt::UserRole).toInt();
    if (orderId > 0) {
        loadOrderDetails(orderId);
        m_contentStack->setCurrentIndex(2);
        m_navigationList->setVisible(false);
    }
}

void ProfilePanel::displayNewMessage(int orderId, const QString& sender, const QString& message)
{
    if (m_currentOrderId == orderId && m_contentStack->currentIndex() == 2) {
        QTextEdit* chatHistory = m_contentStack->widget(2)->findChild<QTextEdit*>("chatHistory");
        if (chatHistory) {
            chatHistory->append(QString("%1: %2").arg(sender, message));
        }
    }
}
