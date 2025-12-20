#pragma once

#include <QDate>
#include <QMainWindow>
#include <memory>
#include <vector>
#include <optional>
#include <QModelIndex>

#include "calendar/data/Event.hpp"
#include "calendar/data/Todo.hpp"

class QToolBar;
class QListView;
class QLineEdit;
class QLabel;
class QShortcut;
class QSplitter;

namespace calendar {
namespace core {
class AppContext;
}

namespace ui {

class TodoListViewModel;
class TodoFilterProxyModel;
class ScheduleViewModel;
class CalendarView;
class EventInlineEditor;
class EventDetailDialog;
class EventPreviewPanel;
class TodoListView;

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
    void saveTodoSplitterState() const;
    void restoreTodoSplitterState();
    void saveCalendarState() const;
    void restoreCalendarState();
    TodoFilterProxyModel *proxyForView(QListView *view) const;
    void handleTodoSelectionChanged(TodoListView *view);
    void clearOtherTodoSelections(QListView *except);
    void updateTodoFilterText(const QString &text);
    void handleTodoStatusDrop(const QList<QUuid> &todoIds, data::TodoStatus status);
    void clearAllTodoSelections();
    void goToday();
    void navigateForward();
    void navigateBackward();
    void refreshTodos();
    void addQuickTodo();
    void deleteSelectedTodos();
    void handleTodoActivated(const QModelIndex &index);
    void refreshCalendar();
    void updateCalendarRange();
    void zoomCalendarHorizontally(bool in);
    void zoomCalendarVertically(bool in);
    void setVisibleDayCount(int days);
    void scrollVisibleDays(int deltaDays);
    void handleEventSelected(const data::CalendarEvent &event);
    void saveEventEdits(const data::CalendarEvent &event);
    void saveTodoEdits(const data::TodoItem &todo);
    void applyEventResize(const QUuid &id, const QDateTime &newStart, const QDateTime &newEnd);
    void clearSelection();
    void handleTodoDropped(const QUuid &todoId, const QDateTime &start);
    void handleEventDropRequested(const QUuid &eventId, const QDateTime &start, bool copy);
    void handleEventDroppedToTodo(const data::CalendarEvent &event, data::TodoStatus status);
    void handleTodoHoverPreview(data::TodoStatus status, const data::CalendarEvent &event);
    void handleTodoHoverCleared();
    void handlePlacementConfirmed(const QDateTime &start);
    void handleHoveredDateTime(const QDateTime &dt);
    void handleEventCreationRequest(const QDateTime &start, const QDateTime &end);
    void openInlineEditor();
    void openDetailDialog();
    void togglePreviewPanel();
    void showPreviewForSelection();
    void showSelectionHint();
    void copySelection();
    void copyTodosAsPlainText(const QList<data::TodoItem> &todos);
    void pasteClipboard();
    void duplicateSelection();
    void pasteClipboardAt(const QDateTime &targetStart);
    QDateTime snapToQuarterHour(const QDateTime &dt) const;
    void cancelPendingPlacement();
    void deleteSelection();
    QDate alignToWeekStart(const QDate &date) const;
    void clearTodoHoverGhosts();
    TodoListView *todoViewForStatus(data::TodoStatus status) const;
    void setInlineEditorActive(bool active);
    void pasteTodosFromPlainText(data::TodoStatus status);
    int insertTodosFromPlainText(const QString &text, data::TodoStatus status);
    QList<data::TodoItem> selectedTodos() const;
    QString todosToPlainText(const QList<data::TodoItem> &todos) const;
    void performUndo();
    void performRedo();

    QWidget *m_todoPanel = nullptr;
    QWidget *m_calendarPanel = nullptr;
    QSplitter *m_todoSplitter = nullptr;
    TodoListView *m_todoPendingView = nullptr;
    TodoListView *m_todoInProgressView = nullptr;
    TodoListView *m_todoDoneView = nullptr;
    QLineEdit *m_todoSearchField = nullptr;
    CalendarView *m_calendarView = nullptr;
    QLabel *m_viewInfoLabel = nullptr;
    EventInlineEditor *m_eventEditor = nullptr;
    EventPreviewPanel *m_previewPanel = nullptr;
    std::unique_ptr<EventDetailDialog> m_eventDetailDialog;
    bool m_previewVisible = false;
    QDate m_currentDate;
    int m_visibleDays = 9;
    double m_savedHourHeight = 60.0;
    int m_savedVerticalScroll = 0;
    std::optional<data::CalendarEvent> m_selectedEvent;
    std::optional<data::TodoItem> m_selectedTodo;
    QDateTime m_lastHoverDateTime;
    std::vector<data::CalendarEvent> m_clipboardEvents;
    bool m_pendingPlacement = false;
    int m_pendingPlacementDuration = 60;
    QString m_pendingPlacementLabel;
    std::unique_ptr<core::AppContext> m_appContext;
    std::unique_ptr<TodoListViewModel> m_todoViewModel;
    std::unique_ptr<TodoFilterProxyModel> m_pendingProxyModel;
    std::unique_ptr<TodoFilterProxyModel> m_inProgressProxyModel;
    std::unique_ptr<TodoFilterProxyModel> m_doneProxyModel;
    std::unique_ptr<ScheduleViewModel> m_scheduleViewModel;
    QAction *m_editEventAction = nullptr;
    QListView *m_activeTodoView = nullptr;
    QShortcut *m_cancelPlacementShortcut = nullptr;
    QString m_eventSearchFilter;
};

} // namespace ui
} // namespace calendar
