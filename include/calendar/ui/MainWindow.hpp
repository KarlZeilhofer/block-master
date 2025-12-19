#pragma once

#include <QMainWindow>
#include <memory>

namespace calendar {
namespace core {
class AppContext;
}

namespace ui {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();
    QWidget *createNavigationBar();
    QWidget *createTodoPanel();
    QWidget *createCalendarPlaceholder();
    void setupShortcuts(QToolBar *toolbar);
    void goToday();
    void navigateForward();
    void navigateBackward();

    QWidget *m_todoPanel = nullptr;
    QWidget *m_calendarPanel = nullptr;
    std::unique_ptr<core::AppContext> m_appContext;
};

} // namespace ui
} // namespace calendar
