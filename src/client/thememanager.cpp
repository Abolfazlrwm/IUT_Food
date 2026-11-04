#include "thememanager.h"

#include <QApplication>
#include <QSettings>

QString ThemeManager::s_current = QStringLiteral("light");

namespace {

const char *kLightTheme = R"(
/* IUT-Food light theme - clean daylight look */
QWidget { background-color: #f7f8fa; color: #1f2430; font-size: 13px; }
QLabel { background: transparent; }
QMainWindow, QDialog { background-color: #f7f8fa; }
QPushButton {
    background-color: #0078d7; color: white; border: none;
    border-radius: 6px; padding: 7px 14px; font-weight: 500;
}
QPushButton:hover { background-color: #0063ae; }
QPushButton:pressed { background-color: #00528f; }
QPushButton:disabled { background-color: #c9ced6; color: #7d838f; }
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QPlainTextEdit {
    background-color: #ffffff; color: #1f2430;
    border: 1px solid #c9ced6; border-radius: 6px; padding: 5px 8px;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QTextEdit:focus { border-color: #0078d7; }
QListWidget, QTableWidget, QTreeWidget {
    background-color: #ffffff; alternate-background-color: #f1f3f6;
    border: 1px solid #c9ced6; border-radius: 6px;
}
QListWidget::item { padding: 6px; }
QListWidget::item:selected, QTableView::item:selected, QTableWidget::item:selected {
    background-color: #0078d7; color: white;
}
QHeaderView::section {
    background-color: #eef0f4; color: #3c4352;
    border: none; border-bottom: 1px solid #c9ced6; padding: 6px;
}
QMenu { background-color: #ffffff; color: #1f2430; border: 1px solid #c9ced6; }
QMenu::item { padding: 6px 24px; }
QMenu::item:selected { background-color: #0078d7; color: white; }
QMenuBar { background-color: #f7f8fa; color: #1f2430; }
QMenuBar::item:selected { background-color: #0078d7; color: white; }
QTabWidget::pane { border: 1px solid #c9ced6; border-radius: 6px; }
QTabBar::tab {
    background-color: #e8eaf0; color: #3c4352; padding: 8px 18px;
    border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px;
}
QTabBar::tab:selected { background-color: #ffffff; font-weight: bold; color: #0078d7; }
QStatusBar { background-color: #eef0f4; color: #3c4352; }
QGroupBox {
    border: 1px solid #c9ced6; border-radius: 6px; margin-top: 14px;
    background-color: #ffffff;
}
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #0078d7; }
QToolBar { background-color: #eef0f4; border: none; spacing: 6px; padding: 4px; }
QComboBox QAbstractItemView {
    background-color: #ffffff; color: #1f2430;
    selection-background-color: #0078d7; selection-color: white;
}
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #b9bfc9; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #969daa; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #b9bfc9; border-radius: 5px; min-width: 30px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QToolTip { background-color: #ffffff; color: #1f2430; border: 1px solid #c9ced6; padding: 4px; }
)";

const char *kDarkTheme = R"(
/* IUT-Food dark theme - midnight campus look */
QWidget { background-color: #1e1f24; color: #e4e6eb; font-size: 13px; }
QLabel { background: transparent; }
QMainWindow, QDialog { background-color: #1e1f24; }
QPushButton {
    background-color: #2f80ed; color: white; border: none;
    border-radius: 6px; padding: 7px 14px; font-weight: 500;
}
QPushButton:hover { background-color: #256ac9; }
QPushButton:pressed { background-color: #1f5cb0; }
QPushButton:disabled { background-color: #35373d; color: #767b85; }
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QPlainTextEdit {
    background-color: #26272c; color: #e4e6eb;
    border: 1px solid #3a3c43; border-radius: 6px; padding: 5px 8px;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QTextEdit:focus { border-color: #2f80ed; }
QListWidget, QTableWidget, QTreeWidget {
    background-color: #232429; alternate-background-color: #26272c;
    border: 1px solid #3a3c43; border-radius: 6px;
}
QListWidget::item { padding: 6px; }
QListWidget::item:selected, QTableView::item:selected, QTableWidget::item:selected {
    background-color: #2f80ed; color: white;
}
QHeaderView::section {
    background-color: #26272c; color: #b9bdc7;
    border: none; border-bottom: 1px solid #3a3c43; padding: 6px;
}
QMenu { background-color: #26272c; color: #e4e6eb; border: 1px solid #3a3c43; }
QMenu::item { padding: 6px 24px; }
QMenu::item:selected { background-color: #2f80ed; color: white; }
QMenuBar { background-color: #1e1f24; color: #e4e6eb; }
QMenuBar::item:selected { background-color: #2f80ed; color: white; }
QTabWidget::pane { border: 1px solid #3a3c43; border-radius: 6px; }
QTabBar::tab {
    background-color: #26272c; color: #b9bdc7; padding: 8px 18px;
    border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px;
}
QTabBar::tab:selected { background-color: #2f80ed; font-weight: bold; color: white; }
QStatusBar { background-color: #181a1e; color: #a9adb6; }
QGroupBox {
    border: 1px solid #3a3c43; border-radius: 6px; margin-top: 14px;
    background-color: #232429;
}
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #7fb2f5; }
QToolBar { background-color: #181a1e; border: none; spacing: 6px; padding: 4px; }
QComboBox QAbstractItemView {
    background-color: #26272c; color: #e4e6eb;
    selection-background-color: #2f80ed; selection-color: white;
}
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #3f414a; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #54565f; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #3f414a; border-radius: 5px; min-width: 30px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QToolTip { background-color: #26272c; color: #e4e6eb; border: 1px solid #3a3c43; padding: 4px; }
)";

} // namespace

void ThemeManager::init()
{
    QSettings settings;
    apply(settings.value(QStringLiteral("theme"), QStringLiteral("light")).toString());
}

void ThemeManager::apply(const QString &name)
{
    s_current = (name == QLatin1String("dark")) ? QStringLiteral("dark") : QStringLiteral("light");
    if (qApp)
        qApp->setStyleSheet(styleSheetFor(s_current));
    QSettings settings;
    settings.setValue(QStringLiteral("theme"), s_current);
}

void ThemeManager::toggle()
{
    apply(isDark() ? QStringLiteral("light") : QStringLiteral("dark"));
}

QString ThemeManager::current()
{
    return s_current;
}

bool ThemeManager::isDark()
{
    return s_current == QLatin1String("dark");
}

QString ThemeManager::styleSheetFor(const QString &name)
{
    return (name == QLatin1String("dark"))
               ? QString::fromUtf8(kDarkTheme)
               : QString::fromUtf8(kLightTheme);
}
