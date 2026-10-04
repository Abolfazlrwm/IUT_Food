#ifndef ORDERSTATUS_H
#define ORDERSTATUS_H

#include <QString>
#include <QLatin1String>

/*!
 * Single source of truth for the order lifecycle.
 *
 * The database and the network protocol only ever store/transmit the
 * canonical English values below; every screen maps them to a Persian
 * label for display through statusToPersian().  Before this file existed
 * the client, the restaurant dashboard and the admin panel each used a
 * different vocabulary (Persian free text vs. English) and orders could
 * never be driven through the full pipeline.
 */
namespace OrderStatus {

inline const QLatin1String Pending   {"pending"};    // placed, waiting for the restaurant
inline const QLatin1String Confirmed {"confirmed"};  // accepted by the restaurant
inline const QLatin1String Shipped   {"shipped"};    // out for delivery
inline const QLatin1String Delivered {"delivered"};  // received by the customer
inline const QLatin1String Cancelled {"cancelled"};  // rejected / cancelled

//! Persian label shown in the UI for a canonical status value.
inline QString toPersian(const QString &canonical)
{
    if (canonical == Pending)      return QStringLiteral("در انتظار تأیید");
    if (canonical == Confirmed)    return QStringLiteral("تأیید شده");
    if (canonical == Shipped)      return QStringLiteral("در حال ارسال");
    if (canonical == Delivered)    return QStringLiteral("تحویل شده");
    if (canonical == Cancelled)    return QStringLiteral("لغو شده");
    return canonical; // unknown values are shown as-is
}

//! True when the value is one of the canonical states above.
inline bool isValid(const QString &canonical)
{
    return canonical == Pending || canonical == Confirmed
        || canonical == Shipped || canonical == Delivered
        || canonical == Cancelled;
}

} // namespace OrderStatus

#endif // ORDERSTATUS_H
