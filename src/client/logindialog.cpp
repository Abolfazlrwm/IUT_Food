#include "logindialog.h"
#include "databasehandler.h"
#include "networkmanager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

LoginDialog::LoginDialog(Mode mode, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_db(&DataBaseHandler::instance())
{
    buildUi();

    setLayoutDirection(Qt::RightToLeft);
    setWindowTitle(mode == Mode::Customer ? QStringLiteral("ورود مشتری")
                                          : mode == Mode::Restaurant
                                                ? QStringLiteral("ورود رستوران")
                                                : QStringLiteral("ورود مدیر سیستم"));
    setFixedSize(420, mode == Mode::Restaurant ? 615 : 385);
}

void LoginDialog::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(m_mode == Mode::Customer
                                 ? QStringLiteral("به IUT-Food خوش آمدید 🍔")
                                 : m_mode == Mode::Restaurant
                                       ? QStringLiteral("پنل مدیریت رستوران 🍕")
                                       : QStringLiteral("پنل مدیریت سیستم 🛡"));
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
    // Registration fields only make sense for the restaurant role; the
    // customer/admin dialogs stay compact.
    restaurantBox->setVisible(m_mode == Mode::Restaurant);

    // v2.3 connection settings: where to reach the server and whether to
    // wrap the channel in TLS.  Persisted so a demo setup survives a restart.
    const QSettings settings;
    auto *connBox = new QGroupBox(QStringLiteral("اتصال به سرور"));
    auto *connForm = new QFormLayout(connBox);
    connForm->setSpacing(8);
    m_hostEdit = new QLineEdit(settings.value(QStringLiteral("connection/host"),
                                              QStringLiteral("127.0.0.1")).toString());
    m_portEdit = new QLineEdit(settings.value(QStringLiteral("connection/port"),
                                              QStringLiteral("1234")).toString());
    m_portEdit->setValidator(new QIntValidator(1, 65535, m_portEdit));
    connForm->addRow(QStringLiteral("آدرس سرور:"), m_hostEdit);
    connForm->addRow(QStringLiteral("درگاه:"), m_portEdit);
    m_tlsCheck = new QCheckBox(QStringLiteral("رمزگذاری TLS 🔒"));
    m_tlsCheck->setChecked(settings.value(QStringLiteral("connection/tls"), false).toBool());
    m_tlsTrustCheck = new QCheckBox(QStringLiteral("پذیرش گواهی self-signed (دمو)"));
    m_tlsTrustCheck->setChecked(settings.value(QStringLiteral("connection/tlsTrust"), true).toBool());
    m_tlsTrustCheck->setEnabled(m_tlsCheck->isChecked());
    connect(m_tlsCheck, &QCheckBox::toggled, m_tlsTrustCheck, &QWidget::setEnabled);
    auto *tlsRow = new QHBoxLayout;
    tlsRow->setContentsMargins(0, 0, 0, 0);
    tlsRow->addWidget(m_tlsCheck);
    tlsRow->addWidget(m_tlsTrustCheck);
    tlsRow->addStretch(1);
    connForm->addRow(QString(), tlsRow);
    layout->addWidget(connBox);

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
    m_registerButton->setVisible(m_mode != Mode::Admin);

    if (m_mode == Mode::Customer)
        m_messageLabel->setText(QStringLiteral("حساب نمونه: demo_customer / 123456"));
    else if (m_mode == Mode::Restaurant)
        m_messageLabel->setText(QStringLiteral("حساب نمونه: kebab_hafez / 123456"));
    else
        m_messageLabel->setText(QStringLiteral("حساب نمونه: admin / admin123"));

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
                                     : m_mode == Mode::Restaurant
                                           ? QStringLiteral("restaurant")
                                           : QStringLiteral("admin");
    if (role != expectedRole) {
        m_messageLabel->setText(QStringLiteral("این حساب برای این بخش نیست."));
        return;
    }

    // Open the authenticated server session for this role as well: the
    // TCP answer carries the session token every later request needs.
    // With the TLS switch on the same JSON lines travel encrypted.
    QSettings settings;
    settings.setValue(QStringLiteral("connection/host"), m_hostEdit->text().trimmed());
    settings.setValue(QStringLiteral("connection/port"), m_portEdit->text().trimmed());
    settings.setValue(QStringLiteral("connection/tls"), m_tlsCheck->isChecked());
    settings.setValue(QStringLiteral("connection/tlsTrust"), m_tlsTrustCheck->isChecked());

    NetworkManager &net = NetworkManager::instance();
    net.connectToServer(m_hostEdit->text().trimmed(),
                        quint16(m_portEdit->text().toInt()),
                        m_tlsCheck->isChecked(),
                        m_tlsTrustCheck->isChecked());
    net.authenticateWith(username, password);

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
    } else if (m_mode == Mode::Restaurant) {
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
