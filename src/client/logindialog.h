#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
class QCheckBox;

class DataBaseHandler;

/*!
 * Sign-in / sign-up dialog shown before entering the customer or the
 * restaurant area.  Before this dialog existed every order was written
 * with a hard-coded customer_id = 1 and the profile screen showed the
 * orders of all accounts.
 */
class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Mode { Customer, Restaurant, Admin };

    explicit LoginDialog(Mode mode, QWidget *parent = nullptr);

    int userId() const { return m_userId; }
    QString username() const { return m_username; }

private slots:
    void onLoginClicked();
    void onRegisterClicked();

private:
    void buildUi();

    Mode m_mode;
    DataBaseHandler *m_db;

    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QComboBox *m_locationCombo = nullptr;
    QComboBox *m_priceCombo = nullptr;
    // v2.3 connection settings
    QLineEdit *m_hostEdit = nullptr;
    QLineEdit *m_portEdit = nullptr;
    QCheckBox *m_tlsCheck = nullptr;
    QCheckBox *m_tlsTrustCheck = nullptr;
    QLabel *m_messageLabel = nullptr;
    QPushButton *m_loginButton = nullptr;
    QPushButton *m_registerButton = nullptr;

    int m_userId = -1;
    QString m_username;
};

#endif // LOGINDIALOG_H
