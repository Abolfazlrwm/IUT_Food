#include "ordertrackerdialog.h"
#include "orderstatus.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

struct StepDef { const char *icon; const char *title; };
const StepDef kSteps[] = {
    {"🧾", "ثبت سفارش"},
    {"👨‍🍳", "تأیید رستوران"},
    {"🛵", "در حال ارسال"},
    {"🍽", "تحویل شده"},
};
constexpr int kStepCount = 4;

QColor stepColor(int index, int activeIndex)
{
    if (index < activeIndex)
        return QColor(0x2e, 0x9e, 0x5b); // done - green
    if (index == activeIndex)
        return QColor(0x2f, 0x80, 0xed); // current - blue
    return QColor(0x9a, 0x9a, 0x9a);     // pending - gray
}

} // namespace

OrderTrackerDialog::OrderTrackerDialog(int orderId, const QString &status, QWidget *parent)
    : QDialog(parent), m_orderId(orderId)
{
    setWindowTitle(QStringLiteral("رهگیری سفارش #%1").arg(orderId));
    setLayoutDirection(Qt::RightToLeft);
    setModal(false);
    setFixedWidth(340);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    layout->addWidget(m_titleLabel);

    for (int i = 0; i < kStepCount; ++i) {
        auto *row = new QHBoxLayout;
        row->setSpacing(10);

        auto *icon = new QLabel(QString::fromUtf8(kSteps[i].icon), this);
        icon->setFixedSize(34, 34);
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet(QStringLiteral(
            "border-radius: 17px; background-color: #d8d8d8; font-size: 15px;"));

        auto *name = new QLabel(QString::fromUtf8(kSteps[i].title), this);
        name->setStyleSheet(QStringLiteral("font-size: 14px;"));

        row->addWidget(icon);
        row->addWidget(name);
        row->addStretch();
        layout->addLayout(row);

        m_stepIcons.append(icon);
        m_stepNames.append(name);
    }

    m_cancelledLabel = new QLabel(this);
    m_cancelledLabel->setStyleSheet(
        QStringLiteral("color: #d93025; font-weight: bold; font-size: 14px;"));
    m_cancelledLabel->hide();
    layout->addWidget(m_cancelledLabel);

    layout->addStretch();
    setStatus(status);
}

void OrderTrackerDialog::setStatus(const QString &status)
{
    const bool cancelled = (status == OrderStatus::Cancelled);
    rebuild(status, cancelled);
}

void OrderTrackerDialog::rebuild(const QString &status, bool cancelled)
{
    int activeIndex = -1;
    if (status == OrderStatus::Pending)      activeIndex = 0;
    else if (status == OrderStatus::Confirmed) activeIndex = 1;
    else if (status == OrderStatus::Shipped)   activeIndex = 2;
    else if (status == OrderStatus::Delivered) activeIndex = 3; // all steps done

    m_titleLabel->setText(QStringLiteral("🚦 رهگیری زنده سفارش #%1").arg(m_orderId));
    m_cancelledLabel->setVisible(cancelled);
    if (cancelled)
        m_cancelledLabel->setText(QStringLiteral("❌ این سفارش لغو شده است."));

    for (int i = 0; i < kStepCount; ++i) {
        const bool done = !cancelled && i <= activeIndex;
        const bool current = !cancelled && i == activeIndex;

        const QColor color = cancelled ? QColor(0x9a, 0x9a, 0x9a) : stepColor(i, activeIndex);
        m_stepIcons[i]->setStyleSheet(QStringLiteral(
            "border-radius: 17px; background-color: %1; font-size: 15px;")
            .arg(color.name()));

        QString label = QString::fromUtf8(kSteps[i].title);
        if (current)
            label += QStringLiteral(" …");
        else if (done && i < activeIndex)
            label += QStringLiteral("  ✓");
        m_stepNames[i]->setText(label);
        m_stepNames[i]->setStyleSheet(QStringLiteral(
            "font-size: 14px; color: %1; font-weight: %2;")
            .arg(cancelled ? QStringLiteral("#8a8a8a")
                           : (done ? QStringLiteral("#1f2430") : QStringLiteral("#8a8a8a")),
                 current ? QStringLiteral("bold") : QStringLiteral("normal")));
    }

    // The last step doubles as the success banner.
    if (!cancelled && activeIndex == kStepCount - 1)
        m_titleLabel->setText(QStringLiteral("🎉 سفارش #%1 تحویل شد!").arg(m_orderId));
}
