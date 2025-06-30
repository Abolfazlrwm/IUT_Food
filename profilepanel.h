#ifndef PROFILEPANEL_H
#define PROFILEPANEL_H

#include <QWidget>

class QListWidget;
class QStackedWidget;
class DataBaseHandler;
class ProfilePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePanel(DataBaseHandler *dbHandler ,QWidget *parent = nullptr);
public slots:
    void refreshHistory();

private:
    void setupUI();
    QWidget* createSettingsPage();
    QWidget* createHistoryPage();

    QListWidget* m_navigationList;
    QStackedWidget* m_contentStack;
    DataBaseHandler *m_dbHandler;
};

#endif // PROFILEPANEL_H
