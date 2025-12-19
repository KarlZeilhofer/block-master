#pragma once

#include <QMainWindow>
#include <QModelIndex>
#include <memory>

class QToolBar;
class QListView;
class QLineEdit;
class QComboBox;

namespace calendar {
namespace core {
class AppContext;
}

namespace ui {

class TodoListViewModel;
class TodoFilterProxyModel;

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
    void refreshTodos();
    void addQuickTodo();
    void deleteSelectedTodos();
    void handleTodoActivated(const QModelIndex &index);
    void updateStatusFilter(int index);

    QWidget *m_todoPanel = nullptr;
    QWidget *m_calendarPanel = nullptr;
    QListView *m_todoListView = nullptr;
    QLineEdit *m_todoSearchField = nullptr;
    QComboBox *m_todoStatusFilter = nullptr;
    std::unique_ptr<core::AppContext> m_appContext;
    std::unique_ptr<TodoListViewModel> m_todoViewModel;
    std::unique_ptr<TodoFilterProxyModel> m_todoProxyModel;
};

} // namespace ui
} // namespace calendar
