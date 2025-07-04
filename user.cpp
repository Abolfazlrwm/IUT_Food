#include "User.h"

User::User(int id, const QString& username, const QString& password,
           const QString& name, const QString& role, QObject* parent)
    : QObject(parent), id(id), username(username), password(password),
    name(name), role(role)
{
}

// Implementations of getters and setters
int User::getId() const { return id; }
QString User::getUsername() const { return username; }
QString User::getPassword() const { return password; }
QString User::getName() const { return name; }
QString User::getRole() const { return role; }

void User::setName(const QString& newName) { name = newName; }
void User::setPassword(const QString& newPassword) { password = newPassword; }
