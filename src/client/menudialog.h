#ifndef MENUDIALOG_H
#define MENUDIALOG_H

#include <QDialog>
#include <QJsonObject>

class DataBaseHandler;

namespace Ui { class MenuDialog; }

//! Shows the menu of one restaurant; each row is a MenuItemWidget that
//! can push dishes into the shared shopping cart.  The header carries a
//! favorite (bookmark) toggle persisted in the favorites table.
class MenuDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MenuDialog(int restaurantId, const QString &restaurantName,
                        DataBaseHandler *dbHandler, int userId = -1,
                        QWidget *parent = nullptr);
    ~MenuDialog() override;

signals:
    void itemAddedToCart(const QJsonObject &foodData, int quantity);

private slots:
    void toggleFavorite();

private:
    void populateMenuItems();
    void refreshFavoriteButton();

    Ui::MenuDialog *ui;
    DataBaseHandler *m_dbHandler;
    int m_restaurantId;
    int m_userId;
    class QPushButton *m_favoriteButton = nullptr;
};

#endif // MENUDIALOG_H
