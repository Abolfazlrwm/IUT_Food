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
    //m_currentRating = 0; // مقداردهی اولیه امتیاز
    setupUI();
}

ProfilePanel::~ProfilePanel(){};


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

// profilepanel.cpp

QWidget* ProfilePanel::createSettingsPage()
{
    QWidget *page = new QWidget;
    QFormLayout *layout = new QFormLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    // ۱. ساخت ویجت‌های UI با objectName مشخص
    QLineEdit *nameEdit = new QLineEdit();
    nameEdit->setObjectName("nameEdit");

    QLineEdit *addressEdit = new QLineEdit();
    addressEdit->setObjectName("addressEdit");

    QPushButton *saveButton = new QPushButton("ذخیره تغییرات");

    layout->addRow("نام کاربری:", nameEdit);
    layout->addRow("آدرس:", addressEdit);
    layout->addRow(saveButton);

    // ۲. بارگذاری اطلاعات فعلی کاربر از دیتابیس
    // توجه: در یک برنامه واقعی، شناسه کاربر باید پس از لاگین ذخیره شود
    int currentUserId = 1; // فعلا شناسه کاربر را ۱ فرض می‌کنیم
    if (m_dbHandler) {
        QSqlQuery query = m_dbHandler->getUserDetails(currentUserId);
        if (query.first()) {
            nameEdit->setText(query.value("username").toString());
            addressEdit->setText(query.value("address").toString());
        }
    }

    // ۳. اتصال دکمه ذخیره به منطق ارسال به سرور
    connect(saveButton, &QPushButton::clicked, this, [this, nameEdit, addressEdit](){
        int currentUserId = 1; // دوباره شناسه کاربر را اینجا داریم

        // خواندن مقادیر جدید از فیلدها
        QString newName = nameEdit->text();
        QString newAddress = addressEdit->text();

        // ساخت درخواست JSON برای سرور
        QJsonObject updateRequest;
        updateRequest["command"] = "update_profile";
        updateRequest["user_id"] = currentUserId;

        QJsonObject data;
        data["username"] = newName;
        data["address"] = newAddress;
        updateRequest["data"] = data;

        // ارسال درخواست به سرور
        NetworkManager::getInstance()->sendJson(updateRequest);

        // آپدیت دیتابیس محلی (کش)
        // در یک سناریوی بهتر، این کار باید پس از دریافت پاسخ موفق از سرور انجام شود
        if (m_dbHandler) {
            m_dbHandler->updateUserDetails(currentUserId, newName, newAddress);
        }

        QMessageBox::information(this, "ارسال شد", "درخواست تغییر اطلاعات برای سرور ارسال شد.");
    });

    return page;
}

// لطفاً این تابع را به طور کامل در profilepanel.cpp جایگزین کنید

#include <QSpinBox> // این هدر را در بالای فایل profilepanel.cpp اضافه کنید

QWidget* ProfilePanel::createOrderDetailPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);

    // بخش‌های دیگر (دکمه بازگشت، اطلاعات سفارش، لیست آیتم‌ها، چت) بدون تغییر باقی می‌مانند
    QPushButton* backButton = new QPushButton(" بازگشت به لیست", page);
    connect(backButton, &QPushButton::clicked, this, [this](){
        m_contentStack->setCurrentIndex(0);
        m_navigationList->setVisible(true);
    });

    QLabel* orderIdLabel = new QLabel("سفارش شماره: -", page);
    orderIdLabel->setObjectName("orderIdLabel");
    QLabel* statusLabel = new QLabel("وضعیت: -", page);
    statusLabel->setObjectName("statusLabel");
    QListWidget* itemsList = new QListWidget(page);
    itemsList->setObjectName("itemsList");
    QTextEdit* chatHistory = new QTextEdit(page);
    chatHistory->setReadOnly(true);
    chatHistory->setObjectName("chatHistory");
    QLineEdit* messageInput = new QLineEdit(page);
    messageInput->setPlaceholderText("پیام خود را بنویسید...");
    QPushButton* sendButton = new QPushButton("ارسال", page);
    connect(sendButton, &QPushButton::clicked, this, [this, messageInput, chatHistory](){
        if(messageInput->text().isEmpty() || m_currentOrderId == -1) return;
        QJsonObject chatRequest;
        chatRequest["command"] = "send_chat_message";
        chatRequest["order_id"] = m_currentOrderId;
        chatRequest["message_text"] = messageInput->text();
        NetworkManager::getInstance()->sendJson(chatRequest);
        chatHistory->append("شما: " + messageInput->text());
        messageInput->clear();
    });

    // ========== بخش امتیازدهی (نسخه نهایی با QSpinBox) ==========
    QGroupBox *ratingBox = new QGroupBox("ثبت امتیاز و نظر", page);
    ratingBox->setObjectName("ratingBox");
    ratingBox->setVisible(false); // در ابتدا مخفی است

    QFormLayout *ratingLayout = new QFormLayout(ratingBox);
    ratingLayout->setContentsMargins(10, 10, 10, 10);
    ratingLayout->setSpacing(10);

    // ۱. ساخت اسپین باکس برای گرفتن امتیاز عددی
    QSpinBox* ratingSpinBox = new QSpinBox(ratingBox);
    ratingSpinBox->setObjectName("ratingSpinBox");
    ratingSpinBox->setRange(1, 5); // امتیاز بین ۱ تا ۵
    ratingSpinBox->setSuffix(" امتیاز");
    ratingSpinBox->setButtonSymbols(QAbstractSpinBox::PlusMinus);

    // ۲. ساخت کادر متن برای نظر
    QTextEdit* reviewTextEdit = new QTextEdit(ratingBox);
    reviewTextEdit->setPlaceholderText("نظر خود را در مورد این سفارش بنویسید...");
    reviewTextEdit->setFixedHeight(80);

    // ۳. ساخت دکمه ثبت نهایی
    QPushButton* submitReviewButton = new QPushButton("ثبت نهایی نظر", ratingBox);

    // اتصال دکمه ثبت نظر
    connect(submitReviewButton, &QPushButton::clicked, this, [this, ratingSpinBox, reviewTextEdit, ratingBox](){
        int rating = ratingSpinBox->value();

        QJsonObject reviewRequest;
        reviewRequest["command"] = "submit_review";
        reviewRequest["order_id"] = m_currentOrderId;
        reviewRequest["rating"] = rating; // ارسال امتیاز عددی
        reviewRequest["review_text"] = reviewTextEdit->toPlainText();

        NetworkManager::getInstance()->sendJson(reviewRequest);

        ratingBox->setEnabled(false);
        ratingBox->setTitle("نظر شما با موفقیت ثبت شد");
        QMessageBox::information(this, "متشکریم", "نظر شما ثبت شد.");
    });

    // اضافه کردن ویجت‌ها به لی‌آوت فرم
    ratingLayout->addRow("امتیاز شما:", ratingSpinBox);
    ratingLayout->addRow("متن نظر:", reviewTextEdit);
    ratingLayout->addRow(submitReviewButton);
    // ===========================================

    // اضافه کردن تمام بخش‌ها به لی‌آوت اصلی صفحه با ترتیب صحیح
    mainLayout->addWidget(backButton);
    mainLayout->addWidget(orderIdLabel);
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(new QLabel("اقلام سفارش:", page));
    mainLayout->addWidget(itemsList);
    mainLayout->addStretch();
    mainLayout->addWidget(new QLabel("چت با رستوران:", page));
    mainLayout->addWidget(chatHistory, 1);
    mainLayout->addWidget(messageInput);
    mainLayout->addWidget(sendButton);
    mainLayout->addWidget(ratingBox); // بخش امتیازدهی در انتها

    return page;
}

// =================================================================
// <<< این توابع حالا در سطح کلاس و خارج از توابع دیگر قرار دارند >>>
// =================================================================

// لطفاً مطمئن شوید تابع شما در profilepanel.cpp به این شکل است

void ProfilePanel::loadOrderDetails(int orderId)
{
    m_currentOrderId = orderId; // شناسه سفارش را ذخیره می‌کنیم

    QWidget* detailPage = m_contentStack->widget(2); // صفحه جزئیات در ایندکس ۲ است
    if (!detailPage || !m_dbHandler) return;

    // پیدا کردن ویجت‌ها با نامشان
    QLabel* idLabel = detailPage->findChild<QLabel*>("orderIdLabel");
    QLabel* statusLabel = detailPage->findChild<QLabel*>("statusLabel");
    QListWidget* itemsList = detailPage->findChild<QListWidget*>("itemsList");
    itemsList->clear();

    QString status;
    bool reviewSubmitted = false; // برای قابلیت "فقط یکبار نظر دادن"

    // گرفتن اطلاعات اصلی سفارش
    QSqlQuery orderQuery = m_dbHandler->getOrderDetails(orderId);
    if (orderQuery.first()) {
        status = orderQuery.value("status").toString();
        reviewSubmitted = orderQuery.value("review_submitted").toBool();
        idLabel->setText("سفارش شماره: " + orderQuery.value("id").toString());
        statusLabel->setText("وضعیت: " + status);
    }

    // گرفتن آیتم‌های سفارش
    QSqlQuery itemsQuery = m_dbHandler->getOrderItems(orderId);
    while(itemsQuery.next()) {
        QString name = itemsQuery.value("name").toString();
        itemsList->addItem(name);
    }

    // پیدا کردن جعبه امتیازدهی و کنترل نمایش آن
    QGroupBox* ratingBox = detailPage->findChild<QGroupBox*>("ratingBox");
    if (ratingBox) {
        if (reviewSubmitted) {
            ratingBox->setVisible(true);
            ratingBox->setEnabled(false);
            ratingBox->setTitle("نظر شما قبلاً برای این سفارش ثبت شده است.");
        }
        else if (status == "تحویل شده") {
            ratingBox->setVisible(true);
            ratingBox->setEnabled(true);
            ratingBox->setTitle("ثبت امتیاز و نظر");
        }
        else {
            ratingBox->setVisible(false);
        }
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
