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
#include <QGroupBox>
#include <QMessageBox>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <QJsonObject>
#include <QIcon>
#include <QSpinBox>


ProfilePanel::ProfilePanel(DataBaseHandler *dbHandler ,QWidget *parent) : QWidget(parent), m_dbHandler(dbHandler)
{
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setFixedSize(550, 450);
    m_currentOrderId = -1;
    m_currentRating = 0;
    m_historyList = nullptr; // مقداردهی اولیه اشاره‌گر جدید
    setupUI();
}

ProfilePanel::~ProfilePanel() {}

// این تابع حالا به درستی لیست را رفرش می‌کند
void ProfilePanel::refreshHistory()
{
    if (!m_historyList || !m_dbHandler) return;

    qDebug() << "Refreshing order history view...";
    m_historyList->clear(); // لیست را پاک می‌کنیم

    QSqlQuery query = m_dbHandler->readAllOrders();
    while (query.next()) {
        int orderId = query.value("id").toInt();
        QString status = query.value("status").toString();
        QString displayText = QString("سفارش شماره %1 - وضعیت: %2").arg(orderId).arg(status);
        QListWidgetItem *item = new QListWidgetItem(displayText, m_historyList);
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
            if(index == 0) refreshHistory(); // با هر بار کلیک روی تاریخچه، لیست رفرش می‌شود
            m_contentStack->setCurrentIndex(index);
        }
    });

    m_navigationList->setCurrentRow(0);
    refreshHistory(); // بارگذاری اولیه لیست تاریخچه
}

QWidget* ProfilePanel::createHistoryPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);

    // حالا m_historyList یک عضو کلاس است
    m_historyList = new QListWidget(page);

    layout->addWidget(new QLabel("تاریخچه سفارشات شما:", page));
    layout->addWidget(m_historyList);
    connect(m_historyList, &QListWidget::itemClicked, this, &ProfilePanel::onHistoryItemClicked);
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

    // بخش چت
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

    // بخش امتیازدهی
    QGroupBox *ratingBox = new QGroupBox("ثبت امتیاز و نظر", page);
    ratingBox->setObjectName("ratingBox");
    ratingBox->setVisible(false);
    QFormLayout *ratingLayout = new QFormLayout(ratingBox);
    QSpinBox* ratingSpinBox = new QSpinBox(ratingBox);
    ratingSpinBox->setRange(1, 5);
    ratingSpinBox->setSuffix(" امتیاز");
    QTextEdit* reviewTextEdit = new QTextEdit(ratingBox);
    reviewTextEdit->setPlaceholderText("نظر خود را بنویسید...");
    QPushButton* submitReviewButton = new QPushButton("ثبت نهایی نظر", ratingBox);
    connect(submitReviewButton, &QPushButton::clicked, this, [this, ratingSpinBox, reviewTextEdit, ratingBox](){
        m_currentRating = ratingSpinBox->value();
        QJsonObject reviewRequest;
        reviewRequest["command"] = "submit_review";
        reviewRequest["order_id"] = m_currentOrderId;
        reviewRequest["rating"] = m_currentRating;
        reviewRequest["review_text"] = reviewTextEdit->toPlainText();
        NetworkManager::getInstance()->sendJson(reviewRequest);
        ratingBox->setEnabled(false);
        ratingBox->setTitle("نظر شما ثبت شد");
    });
    ratingLayout->addRow("امتیاز شما:", ratingSpinBox);
    ratingLayout->addRow("متن نظر:", reviewTextEdit);
    ratingLayout->addRow(submitReviewButton);

    // اضافه کردن ویجت‌ها به لی‌آوت اصلی
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
    mainLayout->addWidget(ratingBox);

    return page;
}

// =================================================================
// <<< این توابع حالا در سطح کلاس و خارج از توابع دیگر قرار دارند >>>
// =================================================================

void ProfilePanel::loadOrderDetails(int orderId)
{
    m_currentOrderId = orderId;
    m_currentRating = 0; // ریست کردن امتیاز برای هر سفارش جدید

    QWidget* detailPage = m_contentStack->widget(2);
    if (!detailPage || !m_dbHandler) return;

    QLabel* idLabel = detailPage->findChild<QLabel*>("orderIdLabel");
    QLabel* statusLabel = detailPage->findChild<QLabel*>("statusLabel");
    QListWidget* itemsList = detailPage->findChild<QListWidget*>("itemsList");
    itemsList->clear();

    QString status;
    QSqlQuery orderQuery = m_dbHandler->getOrderDetails(orderId);
    if (orderQuery.first()) {
        status = orderQuery.value("status").toString();
        idLabel->setText("سفارش شماره: " + orderQuery.value("id").toString());
        statusLabel->setText("وضعیت: " + status);
    }

    QSqlQuery itemsQuery = m_dbHandler->getOrderItems(orderId);
    while(itemsQuery.next()) {
        QString name = itemsQuery.value("name").toString();
        itemsList->addItem(name);
        QSqlQuery itemsQuery = m_dbHandler->getOrderItems(orderId);
        while(itemsQuery.next()) {
            QString name = itemsQuery.value("name").toString();
            qDebug() << "Found item for order" << orderId << ":" << name; // This line is for debugging
            itemsList->addItem(name);
        }
    }

    QGroupBox* ratingBox = detailPage->findChild<QGroupBox*>("ratingBox");
    if (ratingBox) {
        bool isDelivered = (QString::compare(status, "تحویل شده", Qt::CaseInsensitive) == 0);
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
