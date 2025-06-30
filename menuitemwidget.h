#ifndef MENUITEMWIDGET_H
#define MENUITEMWIDGET_H

#include <QWidget>
#include <QJsonObject> // برای ارسال داده غذا

namespace Ui { class MenuItemWidget; }

class MenuItemWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MenuItemWidget(const QJsonObject& foodData, QWidget *parent = nullptr);
    ~MenuItemWidget();

signals:
    void addToCartRequested(const QJsonObject& foodData, int quantity);

private:
    Ui::MenuItemWidget *ui;
    QJsonObject m_foodData;
};
#endif // MENUITEMWIDGET_H
