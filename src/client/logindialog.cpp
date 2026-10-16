#include "logindialog.h"
#include "databasehandler.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

LoginDialog::LoginDialog(Mode mode, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_db(&DataBaseHandler::instance())
{
    buildUi();

    setLayoutDirection(Qt::RightToLeft);
    setWindowTitle(mode == Mode::Customer ? QStringLiteral("ورود مشتری")
                                          : QStringLiteral("ورود رستوران"));
    setFixedSize(420, mode == Mode::Customer ? 260 : 480);
}

void LoginDialog::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(m_mode == Mode::Customer
                                 ? QStringLiteral("به IUT-Food خوش آمدید 🍔")
                                 : QStringLiteral("پنل مدیریت رستوران 🍕"));
    title->setObjectName("titleLabel");
    layout->addWidget(title);

    auto *form = new QFormLayout;
    form->setSpacing(10);
    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setPlaceholderText(QStringLiteral("نام کاربری"));
    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setPlaceholderText(QStringLiteral("رمز عبور"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("نام کاربری:"), m_usernameEdit);
    form->addRow(QStringLiteral("رمز عبور:"), m_passwordEdit);
    layout->addLayout(form);

    // Extra fields only used when creating a new restaurant account.
    auto *restaurantBox = new QGroupBox(QStringLiteral("اطلاعات رستوران (فقط ثبت‌نام)"));
    auto *restaurantForm = new QFormLayout(restaurantBox);
    restaurantForm->setSpacing(8);
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(QStringLiteral("مثلاً: پیتزا پاردیس"));
    m_typeCombo = new QComboBox;
    m_typeCombo->addItems({QStringLiteral("فست فود"), QStringLiteral("ایرانی"),
                           QStringLiteral("ساندویچ"), QStringLiteral("گیلانی")});
    m_locationCombo = new QComboBox;
    m_locationCombo->addItems({QStringLiteral("منطقه یک"), QStringLiteral("منطقه 2")});
    m_priceCombo = new QComboBox;
    m_priceCombo->addItems({QStringLiteral("اقتصادی"), QStringLiteral("متوسط"), QStringLiteral("گران")});
    restaurantForm->addRow(QStringLiteral("نام رستوران:"), m_nameEdit);
    restaurantForm->addRow(QStringLiteral("نوع غذا:"), m_typeCombo);
    restaurantForm->addRow(QStringLiteral("منطقه:"), m_locationCombo);
    restaurantForm->addRow(QStringLiteral("محدوده قیمت:"), m_priceCombo);
    layout->addWidget(restaurantBox);

    m_messageLabel = new QLabel;
    m_messageLabel->setObjectName("messageLabel");
    m_messageLabel->setWordWrap(true);
    layout->addWidget(m_messageLabel);

    auto *buttons = new QHBoxLayout;
    m_loginButton = new QPushButton(QStringLiteral("ورود"));
    m_loginButton->setObjectName("primaryButton");
    m_loginButton->setDefault(true);
    m_registerButton = new QPushButton(m_mode == Mode::Customer
                                           ? QStringLiteral("ایجاد حساب کاربری")
                                           : QStringLiteral("ثبت رستوران"));
    buttons->addWidget(m_loginButton, 1);
    buttons->addWidget(m_registerButton, 1);
    layout->addLayout(buttons);

    if (m_mode == Mode::Customer)
        m_messageLabel->setText(QStringLiteral("حساب نمونه: demo_customer / 123456"));
    else
        m_messageLabel->setText(QStringLiteral("حساب نمونه: kebab_hafez / 123456"));

    connect(m_loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(m_registerButton, &QPushButton::clicked, this, &LoginDialog::onRegisterClicked);
}

void LoginDialog::onLoginClicked()
{
    const QString username = m_usernameEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
        m_messageLabel->setText(QStringLiteral("نام کاربری و رمز عبور را وارد کنید."));
        return;
    }

    QString role;
    int userId = -1;
    if (!m_db->loginUser(username, password, role, userId)) {
        m_messageLabel->setText(QStringLiteral("ورود ناموفق بود؛ اطلاعات را بررسی کنید "
                                               "(یا اگر رستوران تازه ثبت کرده‌اید، منتظر تأیید مدیر بمانید)."));
        return;
    }
    const QString expectedRole = m_mode == Mode::Customer
                                     ? QStringLiteral("customer")
                                     : QStringLiteral("restaurant");
    if (role != expectedRole) {
        m_messageLabel->setText(QStringLiteral("این حساب برای این بخش نیست."));
        return;
    }

    m_userId = userId;
    m_username = username;
    accept();
}

void LoginDialog::onRegisterClicked()
{
    const QString username = m_usernameEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
        m_messageLabel->setText(QStringLiteral("نام کاربری و رمز عبور را وارد کنید."));
        return;
    }
    if (password.size() < 4) {
        m_messageLabel->setText(QStringLiteral("رمز عبور باید حداقل ۴ کاراکتر باشد."));
        return;
    }

    bool ok = false;
    if (m_mode == Mode::Customer) {
        ok = m_db->registerCustomer(username, password,
                                    username, // display name defaults to the username
                                    QString());
    } else {
        if (m_nameEdit->text().trimmed().isEmpty()) {
            m_messageLabel->setText(QStringLiteral("نام رستوران را وارد کنید."));
            return;
        }
        ok = m_db->registerRestaurant(username, password,
                                      m_nameEdit->text().trimmed(),
                                      m_typeCombo->currentText(),
                                      m_locationCombo->currentText(),
                                      m_priceCombo->currentIndex() + 1);
    }

    if (!ok) {
        m_messageLabel->setText(QStringLiteral("این نام کاربری قبلاً استفاده شده است."));
        return;
    }

    m_messageLabel->setText(m_mode == Mode::Customer
                                ? QStringLiteral("حساب ساخته شد؛ حالا وارد شوید.")
                                : QStringLiteral("رستوران ثبت شد؛ پس از تأیید مدیر سیستم می‌توانید وارد شوید."));
}
