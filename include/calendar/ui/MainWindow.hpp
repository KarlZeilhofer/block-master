#pragma once

#include <QDate>
#include <QMainWindow>
#include <QModelIndex>
#include <memory>
#include <vector>

#include "calendar/data/Event.hpp"

class QToolBar;
class QListView;
class QLineEdit;
class QComboBox;
class QLabel;
class QShortcut;

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
    void handleEventSelected(const data::CalendarEvent &event);
    void saveEventEdits(const data::CalendarEvent &event);
    void applyEventResize(const QUuid &id, const QDateTime &newStart, const QDateTime &newEnd);
    void clearSelection();
    void handleTodoDropped(const QUuid &todoId, const QDateTime &start);
    void handleEventDropRequested(const QUuid &eventId, const QDateTime &start, bool copy);
    void handlePlacementConfirmed(const QDateTime &start);
    void handleHoveredDateTime(const QDateTime &dt);
    void openInlineEditor();
    void openDetailDialog();
    void togglePreviewPanel();
    void showPreviewForSelection();
    void showSelectionHint();
    void copySelection();
    void pasteClipboard();
    void duplicateSelection();
    void pasteClipboardAt(const QDateTime &targetStart);
    QDateTime snapToQuarterHour(const QDateTime &dt) const;
    void cancelPendingPlacement();
    void deleteSelection();

    QWidget *m_todoPanel = nullptr;
    QWidget *m_calendarPanel = nullptr;
    QListView *m_todoListView = nullptr;
    QLineEdit *m_todoSearchField = nullptr;
    QComboBox *m_todoStatusFilter = nullptr;
    CalendarView *m_calendarView = nullptr;
    QLabel *m_viewInfoLabel = nullptr;
    EventInlineEditor *m_eventEditor = nullptr;
    EventPreviewPanel *m_previewPanel = nullptr;
    std::unique_ptr<EventDetailDialog> m_eventDetailDialog;
    bool m_previewVisible = false;
    QDate m_currentDate;
    int m_visibleDays = 5;
    data::CalendarEvent m_selectedEvent;
    QDateTime m_lastHoverDateTime;
    std::vector<data::CalendarEvent> m_clipboardEvents;
    bool m_pendingPlacement = false;
    int m_pendingPlacementDuration = 60;
    QString m_pendingPlacementLabel;
    std::unique_ptr<core::AppContext> m_appContext;
    std::unique_ptr<TodoListViewModel> m_todoViewModel;
    std::unique_ptr<TodoFilterProxyModel> m_todoProxyModel;
    std::unique_ptr<ScheduleViewModel> m_scheduleViewModel;
    QAction *m_editEventAction = nullptr;
};

} // namespace ui
} // namespace calendar
