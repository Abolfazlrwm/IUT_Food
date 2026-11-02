#include "menudialog.h"
#include "ui_menudialog.h"
#include "databasehandler.h"
#include "menuitemwidget.h"

#include <QPushButton>

MenuDialog::MenuDialog(int restaurantId, const QString &restaurantName,
                       DataBaseHandler *dbHandler, QWidget *parent)
    : QDialog(parent), ui(new Ui::MenuDialog), m_dbHandler(dbHandler), m_restaurantId(restaurantId)
{
    ui->setupUi(this);
    ui->restaurantNameLabel->setText(
        QStringLiteral("🍽 منوی رستوران %1").arg(restaurantName));
    ui->restaurantNameLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 15px;"));

    if (auto *closeButton = ui->buttonBox->button(QDialogButtonBox::Close))
        closeButton->setText(QStringLiteral("بستن"));
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    populateMenuItems();
}

MenuDialog::~MenuDialog()
{
    delete ui;
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
