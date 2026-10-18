#ifndef MENUITEMWIDGET_H
#define MENUITEMWIDGET_H

#include <QWidget>
#include <QJsonObject>

namespace Ui { class MenuItemWidget; }

//! One row of the restaurant menu: name, price, quantity spinner and an
//! "add to cart" button.
class MenuItemWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MenuItemWidget(const QJsonObject &foodData, QWidget *parent = nullptr);
    ~MenuItemWidget() override;

signals:
    void addToCartRequested(const QJsonObject &foodData, int quantity);

private:
    Ui::MenuItemWidget *ui;
    QJsonObject m_foodData;
};

#endif // MENUITEMWIDGET_H
