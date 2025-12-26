#pragma once

#include <QDialog>

class QListWidget;
class QStackedWidget;

namespace calendar {
namespace ui {

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private:
    void setupUi();
    QWidget *createCalendarPage();
    QWidget *createKeywordPage();

    QListWidget *m_categoryList = nullptr;
    QStackedWidget *m_pages = nullptr;
};

} // namespace ui
} // namespace calendar
