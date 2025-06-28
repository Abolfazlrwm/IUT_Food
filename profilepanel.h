#ifndef PROFILEPANEL_H
#define PROFILEPANEL_H

#include <QWidget>

class QListWidget;
class QStackedWidget;

class ProfilePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePanel(QWidget *parent = nullptr);

private:
    void setupUI();
    QWidget* createSettingsPage();
    QWidget* createHistoryPage();

    QListWidget* m_navigationList;
    QStackedWidget* m_contentStack;
};

#endif // PROFILEPANEL_H
