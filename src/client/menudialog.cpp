#include "menudialog.h"
#include "ui_menudialog.h"
#include "databasehandler.h"
#include "menuitemwidget.h"

#include <QPushButton>
#include <QVBoxLayout>

MenuDialog::MenuDialog(int restaurantId, const QString &restaurantName,
                       DataBaseHandler *dbHandler, int userId, QWidget *parent)
    : QDialog(parent), ui(new Ui::MenuDialog), m_dbHandler(dbHandler),
      m_restaurantId(restaurantId), m_userId(userId)
{
    ui->setupUi(this);
    ui->restaurantNameLabel->setText(
        QStringLiteral("🍽 منوی رستوران %1").arg(restaurantName));
    ui->restaurantNameLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 15px;"));

    if (auto *closeButton = ui->buttonBox->button(QDialogButtonBox::Close))
        closeButton->setText(QStringLiteral("بستن"));
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    // Favorite (bookmark) toggle under the header - only meaningful for a
    // signed-in customer.
    if (m_userId > 0) {
        m_favoriteButton = new QPushButton(this);
        m_favoriteButton->setCursor(Qt::PointingHandCursor);
        ui->verticalLayout->insertWidget(1, m_favoriteButton);
        connect(m_favoriteButton, &QPushButton::clicked,
                this, &MenuDialog::toggleFavorite);
        refreshFavoriteButton();
    }

    populateMenuItems();
}

MenuDialog::~MenuDialog()
{
    delete ui;
}

void MenuDialog::refreshFavoriteButton()
{
    if (!m_dbHandler || !m_favoriteButton)
        return;
    const bool fav = m_dbHandler->isFavorite(m_userId, m_restaurantId);
    m_favoriteButton->setText(fav ? QStringLiteral("⭐ حذف از علاقه‌مندی‌ها")
                                  : QStringLiteral("☆ افزودن به علاقه‌مندی‌ها"));
}

void MenuDialog::toggleFavorite()
{
    if (!m_dbHandler)
        return;
    const bool fav = m_dbHandler->isFavorite(m_userId, m_restaurantId);
    m_dbHandler->setFavorite(m_userId, m_restaurantId, !fav);
    refreshFavoriteButton();
}

void MenuDialog::populateMenuItems()
{
    ui->menuListWidget->clear();
    if (!m_dbHandler || m_restaurantId <= 0)
        return;

    // The `foods` table is the single authoritative menu source; the old
    // build read a `menu_items` table that was never written to.
    const QVector<FoodItem> menu = m_dbHandler->getMenuForRestaurant(m_restaurantId);
    for (const FoodItem &food : menu) {
        QJsonObject foodData;
        foodData[QLatin1String("id")] = food.id;
        foodData[QLatin1String("name")] = food.name;
        foodData[QLatin1String("description")] = food.description;
        foodData[QLatin1String("price")] = food.price;
        foodData[QLatin1String("restaurant_id")] = m_restaurantId;

        auto *item = new QListWidgetItem(ui->menuListWidget);
        auto *widget = new MenuItemWidget(foodData, this);
        connect(widget, &MenuItemWidget::addToCartRequested, this, &MenuDialog::itemAddedToCart);

        item->setSizeHint(widget->sizeHint());
        ui->menuListWidget->setItemWidget(item, widget);
    }
}
