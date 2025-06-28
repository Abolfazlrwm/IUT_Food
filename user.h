#ifndef USER_H
#define USER_H

#include <QString>

class User
{
public:
    enum class Role {
        Customer,
        RestaurantOwner,
        Admin
    };

private:
    int id; // for times when duplication happenes
    QString username;
    QString password;
    QString name;
    Role role;

public:
    User(int id, const QString& username, const QString& password,
         const QString& name, Role role);

    // Destructor
    virtual ~User() = default;

    // Getters
    int getId() const;
    QString getUsername() const;
    QString getPassword() const;
    QString getName() const;
    Role getRole() const;

    // Setters
    void setPassword(const QString& newPassword);
    void setName(const QString& newName);
    void setRole(Role newRole);

    // Utility
    QString roleToString() const;
    static QString roleToString(Role role);
    static Role stringToRole(const QString& str);
};

#endif
