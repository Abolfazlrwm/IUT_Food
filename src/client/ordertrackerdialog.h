#ifndef ORDERTRACKERDIALOG_H
#define ORDERTRACKERDIALOG_H

#include <QDialog>
#include <QList>

class QLabel;

/*!
 * Live order progress window.  The server pushes every status change to
 * the customer session (order_status_update_notification) and the Client
 * feeds it into this dialog, which renders the canonical four-step
 * pipeline from orderstatus.h:
 *
 *   pending -> confirmed -> shipped -> delivered
 *
 * The dialog is non-modal: it stays open on the side and keeps updating
 * while the user keeps browsing.
 */
class OrderTrackerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OrderTrackerDialog(int orderId, const QString &status,
                                QWidget *parent = nullptr);

    //! Re-render the pipeline for the new canonical status.
    void setStatus(const QString &status);
    int orderId() const { return m_orderId; }

private:
    void rebuild(const QString &status, bool cancelled);

    int m_orderId = -1;
    QLabel *m_titleLabel = nullptr;
    QList<QLabel *> m_stepIcons;
    QList<QLabel *> m_stepNames;
    QLabel *m_cancelledLabel = nullptr;
};

#endif // ORDERTRACKERDIALOG_H
