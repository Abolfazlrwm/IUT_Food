#include "restaurantlistdialog.h"
#include "ui_restaurantlistdialog.h"

#include "databasehandler.h"
#include "menudialog.h"
#include "networkmanager.h"
#include "restaurantdelegate.h"
#include "restaurantmodel.h"

#include <QPushButton>

RestaurantListDialog::RestaurantListDialog(const QString &name, const QString &type,
                                           const QString &location, DataBaseHandler *dbHandler,
                                           QWidget *parent)
    : QDialog(parent), ui(new Ui::RestaurantListDialog), m_dbHandler(dbHandler),
      m_nameFilter(name), m_typeFilter(type), m_locationFilter(location)
{
    ui->setupUi(this);
    if (auto *closeButton = ui->buttonBox->button(QDialogButtonBox::Close))
        closeButton->setText(QStringLiteral("بستن"));
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    m_model = new RestaurantModel(this);
    ui->restaurantListView->setModel(m_model);
    ui->restaurantListView->setItemDelegate(new RestaurantDelegate(this));
    ui->restaurantListView->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(ui->restaurantListView, &QListView::clicked,
            this, &RestaurantListDialog::onRestaurantItemClicked);

    // Fill the view immediately from the local database...
    populateModelFromDb();

    // ...then ask the server for the latest list and refresh once the
    // answer arrives.  (The old build sent the request twice, connected
    // the response slot twice and processed every answer twice.)
    connect(&NetworkManager::instance(), &NetworkManager::restaurantsReceived,
            this, &RestaurantListDialog::onRestaurantsReceived);
    if (NetworkManager::instance().isConnected()) {
        QJsonObject request;
        request[QLatin1String("command")] = QLatin1String("get_restaurants");
        request[QLatin1String("name")] = m_nameFilter;
        request[QLatin1String("type")] = m_typeFilter;
        request[QLatin1String("location")] = m_locationFilter;
        NetworkManager::instance().sendJson(request);
    }
}

RestaurantListDialog::~RestaurantListDialog()
{
    delete ui;
}

void RestaurantListDialog::onRestaurantsReceived(const QJsonArray &restaurantsData)
{
    // Refresh the client-side cache with the server answer...
    m_dbHandler->clearRestaurantsTable();
    for (const QJsonValue &value : restaurantsData)
        m_dbHandler->addRestaurant(value.toObject());

    // ...and redraw the view (local rows + newly cached server rows).
    populateModelFromDb();
}

void RestaurantListDialog::populateModelFromDb()
{
    if (!m_dbHandler)
        return;
    QSqlQuery query = m_dbHandler->getAllRestaurants(m_typeFilter, m_locationFilter, m_nameFilter);
    m_model->populateData(query);

    // Server-served restaurants arrive through the cache table; rows that
    // also exist locally are already visible, so only append new ones.
    QSqlQuery cached = m_dbHandler->getCachedRestaurants(m_typeFilter, m_locationFilter, m_nameFilter);
    m_model->appendUnique(cached);
}

void RestaurantListDialog::onRestaurantItemClicked(const QModelIndex &index)
{
    const RestaurantData restaurant = m_model->getRestaurant(index.row());
    if (restaurant.id <= 0)
        return;

    MenuDialog menuDialog(restaurant.id, restaurant.name, m_dbHandler, this);
    connect(&menuDialog, &MenuDialog::itemAddedToCart, this, &RestaurantListDialog::itemAddedToCart);
    menuDialog.exec();
}
