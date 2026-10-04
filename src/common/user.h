#ifndef USER_H
#define USER_H

#include <QObject>
#include <QString>

//! Minimal domain object shared by every account role (customer,
//! restaurant owner, admin).  Restaurant is the only subclass today.
class User : public QObject
{
    Q_OBJECT
public:
    User(int id, const QString &username, const QString &password,
         const QString &name, const QString &role, QObject *parent = nullptr);

    int getId() const;
    QString getUsername() const;
    QString getPassword() const;
    QString getName() const;
    QString getRole() const;

    void setName(const QString &newName);
    void setPassword(const QString &newPassword);

protected:
    int id;
    QString username;
    QString password;
    QString name;
    QString role;
};

#endif // USER_H
