#include "user.h"

#include "User.h"

User::User(int id, const QString& username, const QString& password,
           const QString& name, Role role)
    : id(id), username(username), password(password), name(name), role(role)
{
}

// Getters
int User::getId() const { return id; }
QString User::getUsername() const { return username; }
QString User::getPassword() const { return password; }
QString User::getName() const { return name; }
User::Role User::getRole() const { return role; }

// Setters
void User::setPassword(const QString& newPassword) { password = newPassword; }
void User::setName(const QString& newName) { name = newName; }
void User::setRole(Role newRole) { role = newRole; }

// Utility functions
QString User::roleToString() const {
    return roleToString(role);
}

QString User::roleToString(Role role) {
    switch (role) {
    case Role::Customer:
        return "customer";
    case Role::RestaurantOwner:
        return "restaurant";
    case Role::Admin:
        return "admin";
    default:
        return "unknown";
    }
}

User::Role User::stringToRole(const QString& str) {
    if (str == "customer")
        return Role::Customer;
    else if (str == "restaurant")
        return Role::RestaurantOwner;
    else if (str == "admin")
        return Role::Admin;
    else
        throw std::invalid_argument("Invalid role string");
}
