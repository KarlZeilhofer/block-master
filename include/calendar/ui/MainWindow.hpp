#pragma once

#include <QDate>
#include <QMainWindow>
#include <QModelIndex>
#include <memory>

class QToolBar;
class QListView;
class QLineEdit;
class QComboBox;
class QLabel;

namespace calendar {
namespace core {
class AppContext;
}

namespace ui {

class TodoListViewModel;
class TodoFilterProxyModel;
class ScheduleViewModel;
class CalendarView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();
    QToolBar *createNavigationBar();
    QWidget *createTodoPanel();
    QWidget *createCalendarView();
    void setupShortcuts(QToolBar *toolbar);
    void goToday();
    void navigateForward();
    void navigateBackward();
    void refreshTodos();
    void addQuickTodo();
    void deleteSelectedTodos();
    void handleTodoActivated(const QModelIndex &index);
    void updateStatusFilter(int index);
    void refreshCalendar();
    void updateCalendarRange();
    void zoomCalendarHorizontally(bool in);
    void zoomCalendarVertically(bool in);

    QWidget *m_todoPanel = nullptr;
    QWidget *m_calendarPanel = nullptr;
    QListView *m_todoListView = nullptr;
    QLineEdit *m_todoSearchField = nullptr;
    QComboBox *m_todoStatusFilter = nullptr;
    CalendarView *m_calendarView = nullptr;
    QLabel *m_viewInfoLabel = nullptr;
    QDate m_currentDate;
    int m_visibleDays = 5;
    std::unique_ptr<core::AppContext> m_appContext;
    std::unique_ptr<TodoListViewModel> m_todoViewModel;
    std::unique_ptr<TodoFilterProxyModel> m_todoProxyModel;
    std::unique_ptr<ScheduleViewModel> m_scheduleViewModel;
};

} // namespace ui
} // namespace calendar
