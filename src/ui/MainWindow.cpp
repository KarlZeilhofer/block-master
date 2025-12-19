#include "calendar/ui/MainWindow.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QVariant>
#include <optional>

#include "calendar/core/AppContext.hpp"
#include "calendar/data/Event.hpp"
#include "calendar/data/EventRepository.hpp"
#include "calendar/data/TodoRepository.hpp"
#include "calendar/ui/models/TodoFilterProxyModel.hpp"
#include "calendar/ui/models/TodoListModel.hpp"
#include "calendar/ui/viewmodels/ScheduleViewModel.hpp"
#include "calendar/ui/viewmodels/TodoListViewModel.hpp"
#include "calendar/ui/widgets/CalendarView.hpp"
#include "calendar/ui/widgets/EventInlineEditor.hpp"

namespace calendar {
namespace ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_appContext(std::make_unique<core::AppContext>())
    , m_todoViewModel(std::make_unique<TodoListViewModel>(m_appContext->todoRepository()))
    , m_todoProxyModel(std::make_unique<TodoFilterProxyModel>())
    , m_scheduleViewModel(std::make_unique<ScheduleViewModel>(m_appContext->eventRepository()))
{
    m_currentDate = QDate::currentDate();
    setupUi();
    refreshTodos();
    refreshCalendar();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(tr("Calendar App (Prototype)"));
    resize(1200, 800);

    auto *toolbar = createNavigationBar();
    addToolBar(Qt::TopToolBarArea, toolbar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_todoPanel = createTodoPanel();
    m_calendarPanel = createCalendarView();

    splitter->addWidget(m_todoPanel);
    splitter->addWidget(m_calendarPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setCollapsible(0, true);
    splitter->setCollapsible(1, false);

    auto *centralWidget = new QWidget(this);
    auto *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(splitter);

    setCentralWidget(centralWidget);
    if (m_scheduleViewModel && m_calendarView) {
        connect(m_scheduleViewModel.get(),
                &ScheduleViewModel::eventsChanged,
                this,
                [this](const std::vector<data::CalendarEvent> &events) {
                    if (m_calendarView) {
                        m_calendarView->setEvents(events);
                    }
                });
    }
    updateCalendarRange();
    statusBar()->showMessage(tr("Bereit"));
}

QToolBar *MainWindow::createNavigationBar()
{
    auto *toolbar = new QToolBar(tr("Navigation"), this);
    toolbar->setMovable(false);

    auto *todayAction = toolbar->addAction(tr("Heute"));
    todayAction->setShortcut(QKeySequence(Qt::Key_T));
    connect(todayAction, &QAction::triggered, this, &MainWindow::goToday);

    auto *backAction = toolbar->addAction(tr("Zurück"));
    backAction->setShortcut(QKeySequence(Qt::Key_Left));
    connect(backAction, &QAction::triggered, this, &MainWindow::navigateBackward);

    auto *forwardAction = toolbar->addAction(tr("Weiter"));
    forwardAction->setShortcut(QKeySequence(Qt::Key_Right));
    connect(forwardAction, &QAction::triggered, this, &MainWindow::navigateForward);

    toolbar->addSeparator();

    m_viewInfoLabel = new QLabel(toolbar);
    toolbar->addWidget(m_viewInfoLabel);
    updateCalendarRange();

    auto *zoomDayIn = toolbar->addAction(tr("Tag-Zoom +"));
    zoomDayIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    connect(zoomDayIn, &QAction::triggered, this, [this]() { zoomCalendarHorizontally(true); });

    auto *zoomDayOut = toolbar->addAction(tr("Tag-Zoom -"));
    zoomDayOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(zoomDayOut, &QAction::triggered, this, [this]() { zoomCalendarHorizontally(false); });

    auto *zoomTimeIn = toolbar->addAction(tr("Zeit-Zoom +"));
    zoomTimeIn->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Plus));
    connect(zoomTimeIn, &QAction::triggered, this, [this]() { zoomCalendarVertically(true); });

    auto *zoomTimeOut = toolbar->addAction(tr("Zeit-Zoom -"));
    zoomTimeOut->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Minus));
    connect(zoomTimeOut, &QAction::triggered, this, [this]() { zoomCalendarVertically(false); });

    setupShortcuts(toolbar);
    return toolbar;
}

QWidget *MainWindow::createTodoPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    auto *headerLayout = new QHBoxLayout();
    auto *title = new QLabel(tr("TODOs"), panel);
    title->setObjectName(QStringLiteral("todoTitle"));
    headerLayout->addWidget(title);
    headerLayout->addStretch(1);

    m_todoSearchField = new QLineEdit(panel);
    m_todoSearchField->setPlaceholderText(tr("Suche…"));
    connect(m_todoSearchField, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (m_todoProxyModel) {
            m_todoProxyModel->setFilterText(text);
        }
    });

    m_todoStatusFilter = new QComboBox(panel);
    m_todoStatusFilter->addItem(tr("Alle"), QVariant());
    m_todoStatusFilter->addItem(tr("Offen"), static_cast<int>(data::TodoStatus::Pending));
    m_todoStatusFilter->addItem(tr("In Arbeit"), static_cast<int>(data::TodoStatus::InProgress));
    m_todoStatusFilter->addItem(tr("Erledigt"), static_cast<int>(data::TodoStatus::Completed));
    connect(m_todoStatusFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::updateStatusFilter);

    auto *todoList = new QListView(panel);
    todoList->setObjectName(QStringLiteral("todoList"));
    todoList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    todoList->setSelectionBehavior(QAbstractItemView::SelectRows);
    todoList->setUniformItemSizes(true);
    todoList->setDragEnabled(true);
    todoList->setDragDropMode(QAbstractItemView::DragOnly);
    todoList->setContextMenuPolicy(Qt::ActionsContextMenu);
    todoList->setToolTip(tr("TODO-Liste (Drag & Drop in Kalender)"));
    connect(todoList, &QListView::activated, this, &MainWindow::handleTodoActivated);
    m_todoListView = todoList;

    if (m_todoProxyModel && m_todoViewModel) {
        m_todoProxyModel->setSourceModel(m_todoViewModel->model());
        todoList->setModel(m_todoProxyModel.get());
    }

    auto *deleteAction = new QAction(tr("TODO löschen"), todoList);
    deleteAction->setShortcut(QKeySequence::Delete);
    todoList->addAction(deleteAction);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedTodos);
    addAction(deleteAction);

    layout->addLayout(headerLayout);
    layout->addWidget(m_todoSearchField);
    layout->addWidget(m_todoStatusFilter);

    layout->addWidget(todoList, 1);
    panel->setMinimumWidth(280);
    panel->setMaximumWidth(450);

    return panel;
}

QWidget *MainWindow::createCalendarView()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_calendarView = new CalendarView(panel);
    connect(m_calendarView, &CalendarView::eventActivated, this, [this](const data::CalendarEvent &event) {
        statusBar()->showMessage(tr("Termin: %1, %2").arg(event.title, event.start.toString()), 2500);
    });
    connect(m_calendarView, &CalendarView::hoveredDateTime, this, [this](const QDateTime &dt) {
        statusBar()->showMessage(tr("Cursor: %1").arg(dt.toString()), 1000);
    });
    connect(m_calendarView, &CalendarView::eventSelected, this, &MainWindow::handleEventSelected);
    connect(m_calendarView, &CalendarView::eventResizeRequested, this, &MainWindow::applyEventResize);
    connect(m_calendarView, &CalendarView::selectionCleared, this, [this]() {
        m_selectedEvent = data::CalendarEvent();
        if (m_eventEditor) {
            m_eventEditor->clearEditor();
        }
    });

    m_eventEditor = new EventInlineEditor(panel);
    connect(m_eventEditor, &EventInlineEditor::saveRequested, this, &MainWindow::saveEventEdits);
    connect(m_eventEditor, &EventInlineEditor::cancelRequested, this, [this]() {
        if (m_calendarView) {
            m_calendarView->setFocus();
        }
    });

    layout->addWidget(m_calendarView, 1);
    layout->addWidget(m_eventEditor);
    return panel;
}

void MainWindow::setupShortcuts(QToolBar *toolbar)
{
    auto *newTodoAction = toolbar->addAction(tr("Neues TODO"));
    newTodoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(newTodoAction, &QAction::triggered, this, &MainWindow::addQuickTodo);

    auto *newEventAction = toolbar->addAction(tr("Neuer Termin"));
    newEventAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(newEventAction, &QAction::triggered, this, [this]() {
        statusBar()->showMessage(tr("Termin-Erstellung folgt später"), 2000);
    });

    auto *refreshAction = toolbar->addAction(tr("Aktualisieren"));
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshTodos);
}

void MainWindow::goToday()
{
    m_currentDate = QDate::currentDate();
    updateCalendarRange();
    refreshCalendar();
    statusBar()->showMessage(tr("Heute ausgewählt: %1").arg(m_currentDate.toString(Qt::ISODate)), 2000);
}

void MainWindow::navigateForward()
{
    m_currentDate = m_currentDate.addDays(m_visibleDays);
    updateCalendarRange();
    refreshCalendar();
    statusBar()->showMessage(tr("Weiter: %1").arg(m_currentDate.toString(Qt::ISODate)), 1500);
}

void MainWindow::navigateBackward()
{
    m_currentDate = m_currentDate.addDays(-m_visibleDays);
    updateCalendarRange();
    refreshCalendar();
    statusBar()->showMessage(tr("Zurück: %1").arg(m_currentDate.toString(Qt::ISODate)), 1500);
}

void MainWindow::refreshTodos()
{
    if (!m_todoViewModel) {
        return;
    }
    m_todoViewModel->refresh();
    if (m_todoProxyModel) {
        m_todoProxyModel->invalidate();
    }
}

void MainWindow::addQuickTodo()
{
    bool ok = false;
    const auto title = QInputDialog::getText(this, tr("Neues TODO"), tr("Titel"), QLineEdit::Normal, QString(), &ok);
    if (!ok || title.trimmed().isEmpty()) {
        return;
    }

    data::TodoItem todo;
    todo.title = title.trimmed();
    todo.description = tr("Schnell erfasst");
    todo.priority = 1;
    todo.dueDate = QDateTime::currentDateTime().addDays(1);

    m_appContext->todoRepository().addTodo(std::move(todo));
    refreshTodos();
    statusBar()->showMessage(tr("TODO \"%1\" hinzugefügt").arg(title), 1500);
}

void MainWindow::deleteSelectedTodos()
{
    if (!m_todoListView || !m_todoProxyModel) {
        return;
    }
    const auto indexes = m_todoListView->selectionModel()->selectedIndexes();
    bool removed = false;
    for (const auto &index : indexes) {
        const auto sourceIndex = m_todoProxyModel->mapToSource(index);
        if (const auto *todo = m_todoViewModel->model()->todoAt(sourceIndex)) {
            removed |= m_appContext->todoRepository().removeTodo(todo->id);
        }
    }
    if (removed) {
        refreshTodos();
        statusBar()->showMessage(tr("Ausgewählte TODOs gelöscht"), 1500);
    }
}

void MainWindow::handleTodoActivated(const QModelIndex &index)
{
    if (!m_todoProxyModel) {
        return;
    }
    const auto sourceIndex = m_todoProxyModel->mapToSource(index);
    const auto *todo = m_todoViewModel->model()->todoAt(sourceIndex);
    if (!todo) {
        return;
    }
    statusBar()->showMessage(tr("TODO \"%1\" (Priorität %2)").arg(todo->title).arg(todo->priority), 2000);
}

void MainWindow::updateStatusFilter(int index)
{
    if (!m_todoStatusFilter || !m_todoProxyModel) {
        return;
    }
    const auto data = m_todoStatusFilter->itemData(index);
    if (!data.isValid()) {
        m_todoProxyModel->setStatusFilter(std::nullopt);
        return;
    }
    m_todoProxyModel->setStatusFilter(static_cast<data::TodoStatus>(data.toInt()));
}

void MainWindow::refreshCalendar()
{
    if (!m_scheduleViewModel) {
        return;
    }
    m_scheduleViewModel->refresh();
    if (m_calendarView) {
        m_calendarView->setEvents(m_scheduleViewModel->events());
    }
    if (!m_selectedEvent.id.isNull() && m_eventEditor) {
        if (auto latest = m_appContext->eventRepository().findById(m_selectedEvent.id)) {
            m_selectedEvent = *latest;
            if (m_eventEditor->isVisible()) {
                m_eventEditor->setEvent(*latest);
            }
        }
    }
}

void MainWindow::updateCalendarRange()
{
    if (!m_scheduleViewModel || !m_calendarView) {
        return;
    }
    const QDate end = m_currentDate.addDays(m_visibleDays - 1);
    m_scheduleViewModel->setRange(m_currentDate, end);
    m_calendarView->setDateRange(m_currentDate, m_visibleDays);
    if (m_viewInfoLabel) {
        m_viewInfoLabel->setText(tr("%1 - %2 (%3 Tage)")
                                     .arg(m_currentDate.toString(Qt::ISODate),
                                          end.toString(Qt::ISODate))
                                     .arg(m_visibleDays));
    }
}

void MainWindow::zoomCalendarHorizontally(bool in)
{
    if (in) {
        m_visibleDays = qMax(1, m_visibleDays - 1);
    } else {
        m_visibleDays = qMin(14, m_visibleDays + 1);
    }
    updateCalendarRange();
    refreshCalendar();
}

void MainWindow::zoomCalendarVertically(bool in)
{
    if (!m_calendarView) {
        return;
    }
    m_calendarView->zoomTime(in ? 1.1 : 0.9);
}

void MainWindow::handleEventSelected(const data::CalendarEvent &event)
{
    m_selectedEvent = event;
    if (m_eventEditor) {
        m_eventEditor->setEvent(event);
    }
}

void MainWindow::saveEventEdits(const data::CalendarEvent &event)
{
    auto updated = event;
    m_appContext->eventRepository().updateEvent(updated);
    m_selectedEvent = updated;
    refreshCalendar();
    statusBar()->showMessage(tr("Termin gespeichert: %1").arg(updated.title), 1500);
}

void MainWindow::applyEventResize(const QUuid &id, const QDateTime &newStart, const QDateTime &newEnd)
{
    auto existing = m_appContext->eventRepository().findById(id);
    if (!existing) {
        return;
    }
    existing->start = newStart;
    existing->end = newEnd;
    m_appContext->eventRepository().updateEvent(*existing);
    if (!m_selectedEvent.id.isNull() && m_selectedEvent.id == id && m_eventEditor) {
        m_eventEditor->setEvent(*existing);
    }
    refreshCalendar();
    statusBar()->showMessage(tr("Termin angepasst: %1").arg(existing->title), 1500);
}

} // namespace ui
} // namespace calendar
