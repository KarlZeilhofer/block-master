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
#include <QtMath>
#include <QToolBar>
#include <QToolButton>
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
#include "calendar/ui/widgets/EventPreviewPanel.hpp"
#include "calendar/ui/dialogs/EventDetailDialog.hpp"

namespace calendar {
namespace ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_appContext(std::make_unique<core::AppContext>())
    , m_todoViewModel(std::make_unique<TodoListViewModel>(m_appContext->todoRepository()))
    , m_todoProxyModel(std::make_unique<TodoFilterProxyModel>())
    , m_scheduleViewModel(std::make_unique<ScheduleViewModel>(m_appContext->eventRepository()))
{
    m_currentDate = alignToWeekStart(QDate::currentDate());
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
    deleteAction->setShortcutContext(Qt::WidgetShortcut);
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
    connect(m_calendarView, &CalendarView::hoveredDateTime, this, &MainWindow::handleHoveredDateTime);
    connect(m_calendarView, &CalendarView::eventSelected, this, &MainWindow::handleEventSelected);
    connect(m_calendarView, &CalendarView::eventResizeRequested, this, &MainWindow::applyEventResize);
    connect(m_calendarView, &CalendarView::selectionCleared, this, &MainWindow::clearSelection);
    connect(m_calendarView, &CalendarView::todoDropped, this, &MainWindow::handleTodoDropped);
    connect(m_calendarView, &CalendarView::eventDropRequested, this, &MainWindow::handleEventDropRequested);
    connect(m_calendarView, &CalendarView::externalPlacementConfirmed, this, &MainWindow::handlePlacementConfirmed);
    connect(m_calendarView, &CalendarView::dayZoomRequested, this, &MainWindow::zoomCalendarHorizontally);
    connect(m_calendarView, &CalendarView::dayScrollRequested, this, &MainWindow::scrollVisibleDays);

    m_eventEditor = new EventInlineEditor(panel);
    m_eventEditor->setVisible(false);
    connect(m_eventEditor, &EventInlineEditor::saveRequested, this, &MainWindow::saveEventEdits);
    connect(m_eventEditor, &EventInlineEditor::cancelRequested, this, [this]() {
        if (m_calendarView) {
            m_calendarView->setFocus();
        }
        if (m_eventEditor) {
            m_eventEditor->clearEditor();
        }
    });

    m_previewPanel = new EventPreviewPanel(panel);
    m_previewPanel->setVisible(false);

    layout->addWidget(m_calendarView, 1);

    auto *infoLayout = new QHBoxLayout();
    infoLayout->setContentsMargins(8, 4, 8, 4);
    infoLayout->addStretch();
    auto *infoButton = new QToolButton(panel);
    infoButton->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
    infoButton->setToolTip(tr("Details anzeigen (Space)"));
    infoButton->setAutoRaise(true);
    connect(infoButton, &QToolButton::clicked, this, &MainWindow::togglePreviewPanel);
    infoLayout->addWidget(infoButton);
    layout->addLayout(infoLayout);

    layout->addWidget(m_previewPanel);
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
        data::CalendarEvent event;
        event.start = QDateTime(m_currentDate, QTime(9, 0));
        event.end = event.start.addSecs(60 * 60);
        if (!m_eventDetailDialog) {
            m_eventDetailDialog = std::make_unique<EventDetailDialog>(this);
        }
        m_eventDetailDialog->setEvent(event);
        if (m_eventDetailDialog->exec() == QDialog::Accepted) {
            auto created = m_eventDetailDialog->event();
            m_appContext->eventRepository().addEvent(created);
            refreshCalendar();
            statusBar()->showMessage(tr("Termin erstellt"), 1500);
        }
    });

    auto *refreshAction = toolbar->addAction(tr("Aktualisieren"));
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshTodos);

    m_editEventAction = toolbar->addAction(tr("Erweitert bearbeiten…"));
    m_editEventAction->setEnabled(false);
    connect(m_editEventAction, &QAction::triggered, this, &MainWindow::openDetailDialog);

    auto *inlineShortcut = new QShortcut(QKeySequence(Qt::Key_E), this);
    inlineShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(inlineShortcut, &QShortcut::activated, this, &MainWindow::openInlineEditor);

    auto *detailShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    detailShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(detailShortcut, &QShortcut::activated, this, &MainWindow::openDetailDialog);

    auto *previewShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    previewShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(previewShortcut, &QShortcut::activated, this, &MainWindow::togglePreviewPanel);

    auto *previewShortcutAlt = new QShortcut(QKeySequence(Qt::Key_I), this);
    previewShortcutAlt->setContext(Qt::WidgetWithChildrenShortcut);
    connect(previewShortcutAlt, &QShortcut::activated, this, &MainWindow::togglePreviewPanel);

    auto *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copyShortcut, &QShortcut::activated, this, &MainWindow::copySelection);

    auto *pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    pasteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteShortcut, &QShortcut::activated, this, &MainWindow::pasteClipboard);

    auto *duplicateShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this);
    duplicateShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(duplicateShortcut, &QShortcut::activated, this, &MainWindow::duplicateSelection);

    auto *deleteShortcut = new QShortcut(QKeySequence::Delete, this);
    deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::deleteSelection);

    auto *cancelPasteShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    cancelPasteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(cancelPasteShortcut, &QShortcut::activated, this, &MainWindow::cancelPendingPlacement);
}

void MainWindow::goToday()
{
    m_currentDate = alignToWeekStart(QDate::currentDate());
    updateCalendarRange();
    refreshCalendar();
    statusBar()->showMessage(tr("Heute ausgewählt: %1").arg(m_currentDate.toString(Qt::ISODate)), 2000);
}

void MainWindow::navigateForward()
{
    const QDate aligned = alignToWeekStart(m_currentDate);
    if (m_currentDate != aligned) {
        m_currentDate = aligned;
    } else {
        m_currentDate = m_currentDate.addDays(7);
    }
    updateCalendarRange();
    refreshCalendar();
    statusBar()->showMessage(tr("Weiter: %1").arg(m_currentDate.toString(Qt::ISODate)), 1500);
}

void MainWindow::navigateBackward()
{
    const QDate aligned = alignToWeekStart(m_currentDate);
    if (m_currentDate != aligned) {
        m_currentDate = aligned;
    } else {
        m_currentDate = m_currentDate.addDays(-7);
    }
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
    if (!m_selectedEvent.id.isNull()) {
        if (auto latest = m_appContext->eventRepository().findById(m_selectedEvent.id)) {
            m_selectedEvent = *latest;
            if (m_eventEditor && m_eventEditor->isVisible()) {
                m_eventEditor->setEvent(*latest);
            }
        } else {
            clearSelection();
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
    const int delta = in ? -1 : 1;
    setVisibleDayCount(m_visibleDays + delta);
}

void MainWindow::zoomCalendarVertically(bool in)
{
    if (!m_calendarView) {
        return;
    }
    m_calendarView->zoomTime(in ? 1.1 : 0.9);
}

void MainWindow::scrollVisibleDays(int deltaDays)
{
    if (deltaDays == 0) {
        return;
    }
    m_currentDate = m_currentDate.addDays(deltaDays);
    updateCalendarRange();
    refreshCalendar();
    statusBar()->showMessage(tr("Ansicht verschoben: %1")
                                 .arg(m_currentDate.toString(Qt::ISODate)),
                             1200);
}

void MainWindow::setVisibleDayCount(int days)
{
    const int clamped = qBound(1, days, 31);
    if (clamped == m_visibleDays) {
        return;
    }
    m_visibleDays = clamped;
    updateCalendarRange();
    refreshCalendar();
}

void MainWindow::handleEventSelected(const data::CalendarEvent &event)
{
    m_selectedEvent = event;
    if (m_editEventAction) {
        m_editEventAction->setEnabled(true);
    }
    if (m_previewVisible) {
        showPreviewForSelection();
    }
    showSelectionHint();
}

void MainWindow::saveEventEdits(const data::CalendarEvent &event)
{
    auto updated = event;
    m_appContext->eventRepository().updateEvent(updated);
    m_selectedEvent = updated;
    if (m_eventEditor) {
        m_eventEditor->clearEditor();
    }
    refreshCalendar();
    statusBar()->showMessage(tr("Termin gespeichert: %1").arg(updated.title), 1500);
    if (m_previewVisible) {
        showPreviewForSelection();
    }
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

void MainWindow::clearSelection()
{
    m_selectedEvent = data::CalendarEvent();
    if (m_editEventAction) {
        m_editEventAction->setEnabled(false);
    }
    if (m_eventEditor) {
        m_eventEditor->clearEditor();
    }
    if (m_previewPanel) {
        m_previewPanel->clearEvent();
    }
    m_previewVisible = false;
    cancelPendingPlacement();
}

void MainWindow::handleTodoDropped(const QUuid &todoId, const QDateTime &start)
{
    auto todoOpt = m_appContext->todoRepository().findById(todoId);
    if (!todoOpt.has_value()) {
        statusBar()->showMessage(tr("TODO nicht gefunden"), 2000);
        return;
    }

    data::CalendarEvent event;
    event.title = todoOpt->title;
    event.description = todoOpt->description;
    event.start = start;
    event.end = start.addSecs(60 * 60);
    event.location = tr("Geplant aus TODO");
    m_appContext->eventRepository().addEvent(event);
    m_appContext->todoRepository().removeTodo(todoId);

    refreshTodos();
    refreshCalendar();
    statusBar()->showMessage(tr("TODO \"%1\" eingeplant").arg(todoOpt->title), 2000);
}

void MainWindow::handleEventDropRequested(const QUuid &eventId, const QDateTime &start, bool copy)
{
    auto eventOpt = m_appContext->eventRepository().findById(eventId);
    if (!eventOpt.has_value()) {
        return;
    }
    auto eventData = eventOpt.value();
    const qint64 duration = qMax<qint64>(30 * 60, eventData.start.secsTo(eventData.end));
    eventData.start = start;
    eventData.end = start.addSecs(duration);

    if (copy) {
        eventData.id = QUuid::createUuid();
        m_appContext->eventRepository().addEvent(eventData);
        statusBar()->showMessage(tr("Termin kopiert"), 1500);
    } else {
        m_appContext->eventRepository().updateEvent(eventData);
        statusBar()->showMessage(tr("Termin verschoben"), 1500);
    }
    refreshCalendar();
}

void MainWindow::handlePlacementConfirmed(const QDateTime &start)
{
    if (!m_pendingPlacement) {
        return;
    }
    QDateTime snapped = snapToQuarterHour(start);
    pasteClipboardAt(snapped);
    if (m_calendarView) {
        m_calendarView->cancelPlacementPreview();
    }
    m_pendingPlacement = false;
    statusBar()->showMessage(tr("Termin eingefügt"), 1500);
}

void MainWindow::handleHoveredDateTime(const QDateTime &dt)
{
    m_lastHoverDateTime = dt;
    if (m_pendingPlacement && m_calendarView) {
        m_calendarView->updatePlacementPreview(snapToQuarterHour(dt));
    }
    statusBar()->showMessage(tr("Cursor: %1").arg(dt.toString(Qt::ISODate)), 1000);
}

void MainWindow::openInlineEditor()
{
    if (m_selectedEvent.id.isNull() || !m_eventEditor) {
        return;
    }
    cancelPendingPlacement();
    m_previewVisible = false;
    if (m_previewPanel) {
        m_previewPanel->clearEvent();
    }
    m_eventEditor->setEvent(m_selectedEvent);
    m_eventEditor->setVisible(true);
    m_eventEditor->setFocus();
}

void MainWindow::openDetailDialog()
{
    if (m_selectedEvent.id.isNull()) {
        return;
    }
    if (!m_eventDetailDialog) {
        m_eventDetailDialog = std::make_unique<EventDetailDialog>(this);
    }
    m_eventDetailDialog->setEvent(m_selectedEvent);
    if (m_eventDetailDialog->exec() == QDialog::Accepted) {
        auto updated = m_eventDetailDialog->event();
        m_appContext->eventRepository().updateEvent(updated);
        m_selectedEvent = updated;
        refreshCalendar();
        statusBar()->showMessage(tr("Termin aktualisiert"), 1500);
        if (m_previewVisible) {
            showPreviewForSelection();
        }
    }
}

void MainWindow::togglePreviewPanel()
{
    if (!m_previewPanel || m_selectedEvent.id.isNull()) {
        return;
    }
    cancelPendingPlacement();
    if (m_eventEditor) {
        m_eventEditor->clearEditor();
    }
    m_previewVisible = !m_previewVisible;
    if (m_previewVisible) {
        showPreviewForSelection();
    } else {
        m_previewPanel->clearEvent();
    }
}

void MainWindow::showPreviewForSelection()
{
    if (!m_previewPanel || m_selectedEvent.id.isNull()) {
        return;
    }
    m_previewPanel->setEvent(m_selectedEvent);
    m_previewVisible = true;
}

void MainWindow::showSelectionHint()
{
    statusBar()->showMessage(tr("Shortcuts: ⏎ Details • E Inline • Ctrl+C Copy • Ctrl+V Paste • Ctrl+D Duplizieren • Del Löschen • Space Info"), 3500);
}

void MainWindow::copySelection()
{
    if (m_selectedEvent.id.isNull()) {
        return;
    }
    m_clipboardEvents.clear();
    m_clipboardEvents.push_back(m_selectedEvent);
    statusBar()->showMessage(tr("Termin kopiert: %1").arg(m_selectedEvent.title), 1500);
}

void MainWindow::pasteClipboard()
{
    if (m_clipboardEvents.empty()) {
        return;
    }
    if (!m_calendarView) {
        return;
    }
    cancelPendingPlacement();
    const auto &first = m_clipboardEvents.front();
    m_pendingPlacementDuration = qMax(30, static_cast<int>(first.start.secsTo(first.end) / 60));
    m_pendingPlacementLabel = first.title.isEmpty() ? tr("(Ohne Titel)") : first.title;
    m_pendingPlacement = true;
    QDateTime target = m_lastHoverDateTime.isValid() ? m_lastHoverDateTime : QDateTime(m_currentDate, QTime::currentTime());
    target = snapToQuarterHour(target);
    m_calendarView->beginPlacementPreview(m_pendingPlacementDuration, m_pendingPlacementLabel, target);
    statusBar()->showMessage(tr("Klick zum Einfügen – Esc bricht ab"), 4000);
}

void MainWindow::duplicateSelection()
{
    if (m_selectedEvent.id.isNull()) {
        return;
    }
    m_clipboardEvents = { m_selectedEvent };
    pasteClipboard();
}

void MainWindow::pasteClipboardAt(const QDateTime &targetStart)
{
    if (m_clipboardEvents.empty()) {
        return;
    }
    const auto baseStart = m_clipboardEvents.front().start;
    for (const auto &event : m_clipboardEvents) {
        data::CalendarEvent copy = event;
        copy.id = QUuid::createUuid();
        const qint64 offset = baseStart.secsTo(event.start);
        const qint64 duration = event.start.secsTo(event.end);
        copy.start = targetStart.addSecs(offset);
        copy.end = copy.start.addSecs(duration);
        m_appContext->eventRepository().addEvent(copy);
    }
    refreshCalendar();
    statusBar()->showMessage(tr("%1 Termin(e) eingefügt").arg(m_clipboardEvents.size()), 2000);
}

QDateTime MainWindow::snapToQuarterHour(const QDateTime &dt) const
{
    if (!dt.isValid()) {
        return dt;
    }
    int minutes = dt.time().hour() * 60 + dt.time().minute();
    const double ratio = static_cast<double>(minutes) / 15.0;
    int snapped = static_cast<int>(qRound(ratio) * 15);
    snapped = qBound(0, snapped, 23 * 60 + 45);
    int hour = snapped / 60;
    int minute = snapped % 60;
    return QDateTime(dt.date(), QTime(hour, minute));
}

void MainWindow::cancelPendingPlacement()
{
    if (!m_pendingPlacement) {
        return;
    }
    m_pendingPlacement = false;
    if (m_calendarView) {
        m_calendarView->cancelPlacementPreview();
    }
}

void MainWindow::deleteSelection()
{
    if (m_selectedEvent.id.isNull()) {
        return;
    }
    if (m_eventEditor) {
        m_eventEditor->clearEditor();
    }
    cancelPendingPlacement();
    const bool removed = m_appContext->eventRepository().removeEvent(m_selectedEvent.id);
    if (removed) {
        statusBar()->showMessage(tr("Termin gelöscht"), 2000);
        m_selectedEvent = data::CalendarEvent();
        if (m_previewPanel) {
            m_previewPanel->clearEvent();
        }
        m_previewVisible = false;
        refreshCalendar();
    }
}

QDate MainWindow::alignToWeekStart(const QDate &date) const
{
    if (!date.isValid()) {
        return date;
    }
    const int offset = date.dayOfWeek() - 1;
    return date.addDays(-offset);
}

} // namespace ui
} // namespace calendar
