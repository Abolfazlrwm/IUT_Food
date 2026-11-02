#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVector>
#include <QJsonObject>
#include <QMap>

#include "datatypes.h"
#include "fooditem.h"

/*!
 * Central SQLite data-access layer, shared by the client and the server
 * build.  The class is a singleton because Qt keeps one named connection
 * per process and every screen is single-threaded.
 *
 * Schema (v2):
 *   users        - customers, restaurant owners and admins (role column)
 *   restaurants  - client-side cache of restaurants served by the TCP server
 *   foods        - the authoritative menu of every restaurant
 *   orders       - order headers (canonical status, see orderstatus.h)
 *   order_items  - ordered dishes with a denormalised name snapshot
 *   reviews      - rating + comment, one per delivered order
 */
class DataBaseHandler
{
public:
    static DataBaseHandler &instance();

    bool openDataBase(const QString &path);
    bool isOpen() const;
    bool createTables();

    // ----- users ---------------------------------------------------------
    bool registerUser(const QString &userName, const QString &password, const QString &role);
    bool registerCustomer(const QString &userName, const QString &password,
                          const QString &name, const QString &address);
    bool registerRestaurant(const QString &username, const QString &password,
                            const QString &name, const QString &type,
                            const QString &location, int priceRange);
    QSqlQuery readAllUsers();
    bool deleteUser(int userId);
    bool blockUser(int userId);
    bool unblockUser(int userId);
    bool approveRestaurant(int userId);
    bool disapproveRestaurant(int userId);
    bool loginUser(const QString &userName, const QString &password,
                   QString &role, int &userId);
    QString getUserRole(const QString &username);
    QString getUserName(int userId);
    QSqlQuery getUserDetails(int userId);
    bool updateUserDetails(int userId, const QString &newUsername, const QString &newAddress);
    int getUserId(const QString &username);

    // ----- restaurants ---------------------------------------------------
    QSqlQuery getAllRestaurants(const QString &typeFilter = QString(),
                                const QString &locationFilter = QString(),
                                const QString &nameFilter = QString());
    QSqlQuery readRestaurantDetails(int restaurantId);
    bool updateRestaurantDetails(int restaurantId, const QString &name,
                                 const QString &type, const QString &location,
                                 int priceRange);
    int getOrderCountForRestaurant(int restaurantId);
    bool validateRestaurant(int restaurantId);

    // ----- client-side restaurant cache ----------------------------------
    bool clearRestaurantsTable();
    bool addRestaurant(const QJsonObject &restaurantData);
    QSqlQuery getCachedRestaurants(const QString &typeFilter = QString(),
                                   const QString &locationFilter = QString(),
                                   const QString &nameFilter = QString());

    // ----- menu (foods) ---------------------------------------------------
    bool addFoodItem(int restaurantId, const QString &name,
                     double price, const QString &description);
    bool updateFoodItem(int foodId, const QString &name,
                        double price, const QString &description);
    bool deleteFoodItem(int foodId);
    QVector<FoodItem> getMenuForRestaurant(int restaurantId);

    // ----- orders ----------------------------------------------------------
    QSqlQuery readAllOrders();
    QSqlQuery getOrdersForRestaurant(int restaurantId);
    QSqlQuery getOrdersForCustomer(int customerId);
    QSqlQuery getOrderDetails(int orderId);
    QSqlQuery getOrderItems(int orderId);
    int createNewOrder(const QJsonObject &orderData);   //!< returns the new row id
    bool addOrderItems(int orderId, const QMap<int, CartItem> &items);
    bool updateOrderStatus(int orderId, const QString &newStatus);

    // ----- reviews ----------------------------------------------------------
    bool submitReview(int orderId, int restaurantId, int customerId,
                      int rating, const QString &comment);
    QSqlQuery getReviews();

private:
    DataBaseHandler() = default;
    DataBaseHandler(const DataBaseHandler &) = delete;
    DataBaseHandler &operator=(const DataBaseHandler &) = delete;

    QSqlDatabase m_db;
};

#endif // DATABASEHANDLER_H
