#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVector>
#include <QJsonObject>
#include <QMap>
#include <QHash>

#include "datatypes.h"
#include "fooditem.h"

//! Result of validating a coupon code against an order total.
struct CouponResult
{
    bool ok = false;
    double discount = 0.0;
    int couponId = -1;
    QString message;
};

/*!
 * Central SQLite data-access layer, shared by the client and the server
 * build.  The class is a singleton because Qt keeps one named connection
 * per process and every screen is single-threaded.
 *
 * Schema (v3):
 *   users        - customers, restaurant owners and admins (role column),
 *                  plus a wallet `balance`
 *   restaurants  - client-side cache of restaurants served by the TCP server
 *   foods        - the authoritative menu of every restaurant
 *   orders       - order headers (canonical status, see orderstatus.h),
 *                  plus discount / coupon_code / payment_method
 *   order_items  - ordered dishes with a denormalised name snapshot
 *   reviews      - rating + comment, one per delivered order
 *   wallet_transactions - every wallet top-up and payment (audit trail)
 *   coupons      - discount codes managed by the admin
 *   favorites    - per-user bookmarked restaurants
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
                                const QString &nameFilter = QString(),
                                const QVector<int> &onlyIds = QVector<int>());
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
                                   const QString &nameFilter = QString(),
                                   const QVector<int> &onlyIds = QVector<int>());

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

    // ----- wallet -------------------------------------------------------
    double getWalletBalance(int userId);
    bool chargeWallet(int userId, double amount, const QString &note = QString());
    //! Transactional deduction; fails when the balance is insufficient.
    bool payFromWallet(int userId, double amount, int orderId);
    QSqlQuery getWalletTransactions(int userId);

    // ----- coupons --------------------------------------------------------
    bool createCoupon(const QString &code, const QString &kind, double value,
                      double minOrder, const QString &expiresAt, int maxUses);
    bool setCouponActive(int couponId, bool active);
    bool consumeCoupon(int couponId);
    int findCouponId(const QString &code);
    QSqlQuery getAllCoupons();
    CouponResult validateCoupon(const QString &code, double orderTotal);

    // ----- favorites --------------------------------------------------------
    bool isFavorite(int userId, int restaurantId);
    bool setFavorite(int userId, int restaurantId, bool favorite);
    QVector<int> getFavoriteRestaurantIds(int userId);

    // ----- reviews ----------------------------------------------------------
    bool submitReview(int orderId, int restaurantId, int customerId,
                      int rating, const QString &comment);
    QSqlQuery getReviews();

private:
    DataBaseHandler() = default;
    DataBaseHandler(const DataBaseHandler &) = delete;
    DataBaseHandler &operator=(const DataBaseHandler &) = delete;

    //! Schema migration helper: adds a column when it does not exist yet
    //! so a v1/v2 iut_food.db keeps working after the upgrade.
    bool addColumnIfMissing(const QString &table, const QString &definition);

    //! v2.4: prepared-statement reuse.  Returns a long-lived query that
    //! has already compiled \p sql; later calls just re-bind and exec,
    //! so the hot read paths skip the per-request parse/plan work.
    //! Only queries that are consumed without re-entering the handler
    //! are routed through here, and the server is single-threaded, so
    //! reuse never collides.  The cache is dropped whenever the database
    //! is reopened (statements belong to their connection).
    QSqlQuery &statement(const QString &sql);

    QSqlDatabase m_db;
    QHash<QString, QSqlQuery> m_statements;
};

#endif // DATABASEHANDLER_H
