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
#include <QTextEdit> // <<< هدر فراموش شده
#include <QFormLayout>
#include <QSqlQuery>
#include <QVariant>
#include <QListWidgetItem>
#include <QDebug>
#include <QJsonObject>

ProfilePanel::ProfilePanel(DataBaseHandler *dbHandler ,QWidget *parent) : QWidget(parent), m_dbHandler(dbHandler)
{
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setFixedSize(550, 400); // کمی ارتفاع را بیشتر کردم برای جای دادن بخش چت
    m_currentOrderId = -1;
    setupUI();
}

void ProfilePanel::refreshHistory()
{
    // این تابع لیست تاریخچه را در صفحه اول استک، دوباره بارگذاری می‌کند
    QWidget* historyPage = m_contentStack->widget(0);
    QListWidget* historyList = historyPage->findChild<QListWidget*>();
    if(!historyList || !m_dbHandler) return;

    historyList->clear();
    QSqlQuery query = m_dbHandler->readAllOrders();
    bool hasOrders = query.first();
    if (hasOrders) {
        query.previous();
        while (query.next()) {
            int orderId = query.value("id").toInt();
            QString status = query.value("status").toString();
            QString displayText = QString("سفارش شماره %1 - وضعیت: %2").arg(orderId).arg(status);
            QListWidgetItem *item = new QListWidgetItem(displayText, historyList);
            item->setData(Qt::UserRole, orderId);
        }
    } else {
        historyList->addItem("هیچ سفارشی تاکنون ثبت نشده است.");
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
    m_contentStack->addWidget(createHistoryPage());   // ایندکس 0
    m_contentStack->addWidget(createSettingsPage());  // ایندکس 1
    m_contentStack->addWidget(createOrderDetailPage()); // ایندکس 2

    mainLayout->addWidget(m_contentStack, 1);
    mainLayout->addWidget(m_navigationList);

    connect(m_navigationList, &QListWidget::currentRowChanged, this, [this](int index){
        if (index == 0 || index == 1) { // اگر کاربر روی تاریخچه یا تنظیمات کلیک کرد
            m_navigationList->setVisible(true);
            if(index == 0) refreshHistory(); // با کلیک روی تاریخچه، لیست را رفرش کن
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

QWidget* ProfilePanel::createOrderDetailPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);

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

    QLabel* chatTitle = new QLabel("چت با رستوران:", page);
    QTextEdit* chatHistory = new QTextEdit(page); // <<< اصلاح خطای تایپی
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

        // نمایش پیام ارسال شده توسط خود کاربر در صفحه چت
        chatHistory->append("شما: " + messageInput->text());
        messageInput->clear();
    });

    QGroupBox *ratingBox = new QGroupBox("ثبت امتیاز و نظر", page);
    ratingBox->setObjectName("ratingBox"); // نام‌گذاری برای دسترسی بعدی
    ratingBox->setVisible(false); // در ابتدا مخفی باشد

    QVBoxLayout *ratingLayout = new QVBoxLayout(ratingBox);

    // بخش ستاره‌ها برای امتیاز
    QHBoxLayout *starsLayout = new QHBoxLayout();
    for (int i = 1; i <= 5; ++i) {
        QPushButton *starButton = new QPushButton(QIcon(":/icons/star_empty.png"), "", ratingBox); // فرض می‌کنیم آیکون ستاره دارید
        starButton->setObjectName(QString("starButton%1").arg(i));
        starButton->setFixedSize(32, 32);
        starButton->setIconSize(QSize(28, 28));
        starsLayout->addWidget(starButton);
    }
    starsLayout->addStretch();

    QTextEdit* reviewTextEdit = new QTextEdit(ratingBox);
    reviewTextEdit->setObjectName("reviewTextEdit");
    reviewTextEdit->setPlaceholderText("نظر خود را در مورد این سفارش بنویسید...");

    QPushButton* submitReviewButton = new QPushButton("ثبت نهایی نظر", ratingBox);
    submitReviewButton->setObjectName("submitReviewButton");

    ratingLayout->addLayout(starsLayout);
    ratingLayout->addWidget(reviewTextEdit);
    ratingLayout->addWidget(submitReviewButton);
    // ===========================================

    // بخش جدید را به لی‌آوت اصلی صفحه اضافه می‌کنیم
    mainLayout->addWidget(ratingBox);
    mainLayout->addWidget(backButton);
    mainLayout->addWidget(orderIdLabel);
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(new QLabel("اقلام سفارش:", page));
    mainLayout->addWidget(itemsList);
    mainLayout->addStretch();
    mainLayout->addWidget(chatTitle);
    mainLayout->addWidget(chatHistory, 1);
    mainLayout->addWidget(messageInput);
    mainLayout->addWidget(sendButton);

    return page;
}

void ProfilePanel::loadOrderDetails(int orderId)
{
    m_currentOrderId = orderId; // شناسه سفارش فعلی را ذخیره می‌کنیم

    QWidget* detailPage = m_contentStack->widget(2); // صفحه جزئیات در ایندکس ۲ است
    if (!detailPage || !m_dbHandler) return;

    // پیدا کردن ویجت‌ها با نامشان
    QLabel* idLabel = detailPage->findChild<QLabel*>("orderIdLabel");
    QLabel* statusLabel = detailPage->findChild<QLabel*>("statusLabel");
    QListWidget* itemsList = detailPage->findChild<QListWidget*>("itemsList");
    QTextEdit* chatHistory = detailPage->findChild<QTextEdit*>("chatHistory");

    itemsList->clear();
    chatHistory->clear(); // تاریخچه چت را هم پاک می‌کنیم

    // --- ۱. پر کردن اطلاعات اصلی سفارش ---
    QSqlQuery orderQuery = m_dbHandler->getOrderDetails(orderId);
    if (orderQuery.first()) {
        idLabel->setText("سفارش شماره: " + orderQuery.value("id").toString());
        statusLabel->setText("وضعیت: " + orderQuery.value("status").toString());
        // TODO: می‌توانید سایر اطلاعات مثل قیمت کل را هم نمایش دهید
    }

    // --- ۲. پر کردن لیست آیتم‌های داخل سفارش ---
    QSqlQuery itemsQuery = m_dbHandler->getOrderItems(orderId);
    while(itemsQuery.next()) {
        QString name = itemsQuery.value("name").toString();
        int quantity = itemsQuery.value("quantity").toInt();
        double price = itemsQuery.value("price_per_item").toDouble();
        QString displayText = QString("%1 (تعداد: %2) - قیمت واحد: %3")
                                  .arg(name).arg(quantity).arg(price);
        itemsList->addItem(displayText);
    }

    // TODO: در اینجا باید تاریخچه چت مربوط به این سفارش را از سرور درخواست کنیم
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
    // فقط اگر کاربر در حال مشاهده همان سفارش باشد، پیام را نمایش بده
    if (m_currentOrderId == orderId && m_contentStack->currentIndex() == 2) {
        QTextEdit* chatHistory = m_contentStack->widget(2)->findChild<QTextEdit*>("chatHistory");
        if (chatHistory) {
            chatHistory->append(QString("%1: %2").arg(sender, message));
        }
    }
}
