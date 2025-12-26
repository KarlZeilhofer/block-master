#include "calendar/ui/MainWindow.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLocale>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSettings>
#include <QStatusBar>
#include <QStringList>
#include <QtMath>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QVariant>
#include <optional>
#include <algorithm>

#include "calendar/core/AppContext.hpp"
#include "calendar/core/UndoCommand.hpp"
#include "calendar/core/UndoStack.hpp"
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
#include "calendar/ui/widgets/TodoListView.hpp"
#include "calendar/ui/dialogs/EventDetailDialog.hpp"
#include "calendar/ui/dialogs/SettingsDialog.hpp"

namespace {

int placementOffsetMinutes(int durationMinutes)
{
    if (durationMinutes <= 0) {
        return 0;
    }
    if (durationMinutes > 16 * 60) {
        return 8 * 60;
    }
    return durationMinutes / 2;
}

QDateTime applyPlacementAnchor(const QDateTime &target, int durationMinutes)
{
    return target.addSecs(-placementOffsetMinutes(durationMinutes) * 60);
}

struct PlainTextTodoDefinition
{
    QString title;
    QString description;
    QString location;
    int durationMinutes = 0;
};

QString stripControlChars(const QString &value)
{
    static const QRegularExpression controlPattern(QStringLiteral("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F]"));
    QString cleaned = value;
    cleaned.remove(controlPattern);
    cleaned.replace('\t', QStringLiteral(" "));
    return cleaned;
}

int extractDurationFromTitle(QString &title)
{
    static const QRegularExpression durationPattern(QStringLiteral("\\b(\\d+[\\.,]?\\d*)\\s*(h|std|stunden|min|minute|minuten)\\b"),
                                                    QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = durationPattern.match(title);
    if (!match.hasMatch()) {
        return 0;
    }
    QString numberPart = match.captured(1);
    QString unit = match.captured(2).toLower();
    numberPart.replace(',', '.');
    double value = numberPart.toDouble();
    if (value <= 0.0) {
        title.remove(match.capturedStart(), match.capturedLength());
        title = title.trimmed();
        return 0;
    }
    int minutes = 0;
    if (unit.startsWith('h') || unit.startsWith("std")) {
        minutes = static_cast<int>(qRound(value * 60.0));
    } else {
        minutes = static_cast<int>(qRound(value));
    }
    title.remove(match.capturedStart(), match.capturedLength());
    title = title.trimmed();
    title.replace(QRegularExpression(QStringLiteral("\\s{2,}")), QStringLiteral(" "));
    return minutes;
}

QVector<PlainTextTodoDefinition> parsePlainTextTodos(const QString &text)
{
    QVector<PlainTextTodoDefinition> todos;
    if (text.trimmed().isEmpty()) {
        return todos;
    }
    QString normalized = text;
    normalized.replace('\r', QString());
    const QStringList lines = normalized.split('\n');
    PlainTextTodoDefinition current;
    QStringList descriptionLines;
    bool hasCurrent = false;

    auto flushCurrent = [&]() {
        QString title = stripControlChars(current.title).trimmed();
        if (title.isEmpty()) {
            current = PlainTextTodoDefinition();
            descriptionLines.clear();
            hasCurrent = false;
            return;
        }
        PlainTextTodoDefinition entry;
        entry.title = title;
        entry.durationMinutes = current.durationMinutes;
        entry.location = stripControlChars(current.location).trimmed();
        QString description = stripControlChars(descriptionLines.join('\n')).trimmed();
        entry.description = description;
        todos.push_back(entry);
        current = PlainTextTodoDefinition();
        descriptionLines.clear();
        hasCurrent = false;
    };

    for (const QString &rawLine : lines) {
        QString line = rawLine;
        if (line.isEmpty()) {
            continue;
        }
        bool indented = !line.isEmpty() && (line.at(0) == QLatin1Char(' ') || line.at(0) == QLatin1Char('\t'));
        if (!indented) {
            if (!line.trimmed().isEmpty()) {
                if (hasCurrent) {
                    flushCurrent();
                }
                current = PlainTextTodoDefinition();
                descriptionLines.clear();
                current.title = stripControlChars(line).trimmed();
                current.durationMinutes = extractDurationFromTitle(current.title);
                if (!current.title.isEmpty()) {
                    hasCurrent = true;
                }
            }
            continue;
        }
        if (!hasCurrent) {
            continue;
        }
        int index = 0;
        while (index < line.size() && (line.at(index) == QLatin1Char(' ') || line.at(index) == QLatin1Char('\t'))) {
            ++index;
        }
        QString content = stripControlChars(line.mid(index)).trimmed();
        if (content.isEmpty()) {
            continue;
        }
        if (content.startsWith(QStringLiteral("Ort"), Qt::CaseInsensitive)) {
            const int colonIndex = content.indexOf(QLatin1Char(':'));
            if (colonIndex != -1) {
                QString locationValue = stripControlChars(content.mid(colonIndex + 1)).trimmed();
                if (!locationValue.isEmpty()) {
                    current.location = locationValue;
                }
                continue;
            }
        }
        descriptionLines << content;
    }

    if (hasCurrent) {
        flushCurrent();
    }
    return todos;
}

QString durationTokenForMinutes(int minutes)
{
    if (minutes <= 0) {
        return QString();
    }
    if (minutes % 60 == 0) {
        return QStringLiteral("%1h").arg(minutes / 60);
    }
    if (minutes < 60) {
        return QStringLiteral("%1min").arg(minutes);
    }
    double hours = static_cast<double>(minutes) / 60.0;
    QString text = QLocale().toString(hours, 'f', 2);
    text.remove(QRegularExpression(QStringLiteral("(,|\\.)00$")));
    text.remove(QRegularExpression(QStringLiteral("(,|\\.)0$")));
    text.replace('.', ',');
    return QStringLiteral("%1h").arg(text);
}

class PlainTextInsertCommand : public calendar::core::UndoCommand
{
public:
    PlainTextInsertCommand(calendar::data::TodoRepository &repository,
                           std::vector<calendar::data::TodoItem> templates)
        : m_repository(repository)
        , m_templates(std::move(templates))
    {
    }

    void redo() override
    {
        m_createdItems.clear();
        for (auto item : m_templates) {
            item.id = QUuid();
            m_createdItems.push_back(m_repository.addTodo(item));
        }
    }

    void undo() override
    {
        for (const auto &item : m_createdItems) {
            m_repository.removeTodo(item.id);
        }
    }

    std::size_t count() const { return m_templates.size(); }

private:
    calendar::data::TodoRepository &m_repository;
    std::vector<calendar::data::TodoItem> m_templates;
    std::vector<calendar::data::TodoItem> m_createdItems;
};

} // namespace

namespace calendar {
namespace ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_appContext(std::make_unique<core::AppContext>())
    , m_todoViewModel(std::make_unique<TodoListViewModel>(m_appContext->todoRepository()))
    , m_pendingProxyModel(std::make_unique<TodoFilterProxyModel>())
    , m_inProgressProxyModel(std::make_unique<TodoFilterProxyModel>())
    , m_doneProxyModel(std::make_unique<TodoFilterProxyModel>())
    , m_scheduleViewModel(std::make_unique<ScheduleViewModel>(m_appContext->eventRepository()))
{
    if (m_todoViewModel) {
        auto *model = m_todoViewModel->model();
        if (m_pendingProxyModel) {
            m_pendingProxyModel->setSourceModel(model);
            m_pendingProxyModel->setStatusFilter(std::optional<data::TodoStatus>(data::TodoStatus::Pending));
        }
        if (m_inProgressProxyModel) {
            m_inProgressProxyModel->setSourceModel(model);
            m_inProgressProxyModel->setStatusFilter(std::optional<data::TodoStatus>(data::TodoStatus::InProgress));
        }
        if (m_doneProxyModel) {
            m_doneProxyModel->setSourceModel(model);
            m_doneProxyModel->setStatusFilter(std::optional<data::TodoStatus>(data::TodoStatus::Completed));
        }
    }
    m_currentDate = alignToWeekStart(QDate::currentDate());
    m_dayOffset = 0.0;
    m_dayOffset = 0.0;
    restoreCalendarState();
    setupUi();
    refreshTodos();
    refreshCalendar();
}

MainWindow::~MainWindow()
{
    saveCalendarState();
    saveTodoSplitterState();
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("Block Master"));
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
    if (statusBar() && !m_shortcutLabel) {
        m_shortcutLabel = new QLabel(tr("Shortcuts: ⏎ Details • E Inline • Ctrl+C Copy • Ctrl+V Paste • Ctrl+D Duplizieren • Del Löschen • Space Info"),
                                     this);
        m_shortcutLabel->setObjectName(QStringLiteral("shortcutHintLabel"));
        statusBar()->addPermanentWidget(m_shortcutLabel, 1);
        statusBar()->showMessage(tr("Bereit"));
    } else if (statusBar()) {
        statusBar()->showMessage(tr("Bereit"));
    }
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

    auto *spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    auto *settingsButton = new QToolButton(toolbar);
    settingsButton->setText(QString::fromUtf8("\xE2\x98\xB0"));
    settingsButton->setToolTip(tr("Einstellungen"));
    settingsButton->setAutoRaise(true);
    connect(settingsButton, &QToolButton::clicked, this, &MainWindow::openSettingsDialog);
    toolbar->addWidget(settingsButton);

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
        updateTodoFilterText(text);
        m_eventSearchFilter = text;
        if (m_calendarView) {
            m_calendarView->setEventSearchFilter(text);
        }
    });
    m_todoSearchField->installEventFilter(this);
    m_eventSearchFilter = m_todoSearchField->text();

    layout->addLayout(headerLayout);
    layout->addWidget(m_todoSearchField);

    m_todoSplitter = new QSplitter(Qt::Vertical, panel);
    m_todoSplitter->setObjectName(QStringLiteral("todoSplitter"));

    auto createSection = [this, panel](const QString &sectionTitle,
                                       TodoListView *&view,
                                       TodoFilterProxyModel *proxy,
                                       data::TodoStatus status,
                                       bool completed) {
        auto *container = new QWidget(panel);
        auto *sectionLayout = new QVBoxLayout(container);
        sectionLayout->setContentsMargins(0, 0, 0, 0);
        sectionLayout->setSpacing(2);

        auto *label = new QLabel(sectionTitle, container);
        QFont labelFont = label->font();
        labelFont.setBold(true);
        label->setFont(labelFont);
        sectionLayout->addWidget(label);

        view = new TodoListView(container);
        view->setObjectName(QStringLiteral("todoList"));
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setUniformItemSizes(true);
        view->setDragDropMode(QAbstractItemView::DragDrop);
        view->setToolTip(tr("TODO-Liste (Drag & Drop in Kalender)"));
        view->setTargetStatus(status);
        if (proxy) {
            view->setModel(proxy);
            connect(view->selectionModel(),
                    &QItemSelectionModel::selectionChanged,
                    this,
                    [this, view](const QItemSelection &, const QItemSelection &) {
                        handleTodoSelectionChanged(view);
                    });
        }
        connect(view, &TodoListView::activated, this, &MainWindow::handleTodoActivated);
        connect(view, &TodoListView::doubleClicked, this, &MainWindow::handleTodoDoubleClicked);
        connect(view, &TodoListView::todosDropped, this, &MainWindow::handleTodoStatusDrop);
        connect(view, &TodoListView::deleteRequested, this, &MainWindow::deleteSelectedTodos);
        view->setContextMenuPolicy(Qt::ActionsContextMenu);
        if (completed) {
            QPalette pal = view->palette();
            pal.setColor(QPalette::Text, QColor(130, 130, 130));
            view->setPalette(pal);
        }

        auto *deleteAction = new QAction(tr("TODO löschen"), view);
        deleteAction->setShortcut(QKeySequence::Delete);
        deleteAction->setShortcutContext(Qt::WidgetShortcut);
        view->addAction(deleteAction);
        connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedTodos);

        auto *plainPasteAction = new QAction(tr("Einfügen aus Plaintext"), view);
        view->addAction(plainPasteAction);
        connect(plainPasteAction, &QAction::triggered, this, [this, status]() {
            pasteTodosFromPlainText(status);
        });

        sectionLayout->addWidget(view, 1);
        m_todoSplitter->addWidget(container);
    };

    createSection(tr("Offen"), m_todoPendingView, m_pendingProxyModel.get(), data::TodoStatus::Pending, false);
    createSection(tr("In Arbeit"), m_todoInProgressView, m_inProgressProxyModel.get(), data::TodoStatus::InProgress, false);
    createSection(tr("Erledigt"), m_todoDoneView, m_doneProxyModel.get(), data::TodoStatus::Completed, true);

    layout->addWidget(m_todoSplitter, 1);
    panel->setMinimumWidth(300);
    panel->setMaximumWidth(520);

    restoreTodoSplitterState();

    return panel;
}

QWidget *MainWindow::createCalendarView()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_calendarView = new CalendarView(panel);
    m_calendarView->setEventSearchFilter(m_eventSearchFilter);
    m_calendarView->setHourHeight(m_savedHourHeight);
    m_calendarView->setVerticalScrollValue(m_savedVerticalScroll);
    connect(m_calendarView, &CalendarView::eventActivated, this, [this](const data::CalendarEvent &event) {
        statusBar()->showMessage(tr("Termin: %1, %2").arg(event.title, event.start.toString()), 2500);
    });
    connect(m_calendarView, &CalendarView::hoveredDateTime, this, &MainWindow::handleHoveredDateTime);
    connect(m_calendarView, &CalendarView::eventSelected, this, &MainWindow::handleEventSelected);
    connect(m_calendarView, &CalendarView::eventResizeRequested, this, &MainWindow::applyEventResize);
    connect(m_calendarView, &CalendarView::selectionCleared, this, &MainWindow::handleCalendarSelectionCleared);
    connect(m_calendarView, &CalendarView::inlineEditRequested, this, &MainWindow::handleInlineEditRequest);
    connect(m_calendarView, &CalendarView::todoDropped, this, &MainWindow::handleTodoDropped);
    connect(m_calendarView, &CalendarView::eventDropRequested, this, &MainWindow::handleEventDropRequested);
    connect(m_calendarView, &CalendarView::externalPlacementConfirmed, this, &MainWindow::handlePlacementConfirmed);
    connect(m_calendarView, &CalendarView::dayZoomRequested, this, &MainWindow::zoomCalendarHorizontally);
    connect(m_calendarView, &CalendarView::dayScrollRequested, this, &MainWindow::scrollVisibleDays);
    connect(m_calendarView, &CalendarView::fractionalDayScrollRequested, this, &MainWindow::scrollVisibleDaysFractional);
    connect(m_calendarView, &CalendarView::eventDroppedToTodo, this, &MainWindow::handleEventDroppedToTodo);
    connect(m_calendarView, &CalendarView::todoHoverPreviewRequested, this, &MainWindow::handleTodoHoverPreview);
    connect(m_calendarView, &CalendarView::todoHoverPreviewCleared, this, &MainWindow::handleTodoHoverCleared);
    connect(m_calendarView,
            &CalendarView::eventCreationRequested,
            this,
            &MainWindow::handleEventCreationRequest);

    m_eventEditor = new EventInlineEditor(panel);
    m_eventEditor->setVisible(false);
    connect(m_eventEditor, &EventInlineEditor::saveRequested, this, &MainWindow::saveEventEdits);
    connect(m_eventEditor, &EventInlineEditor::saveTodoRequested, this, &MainWindow::saveTodoEdits);
    connect(m_eventEditor, &EventInlineEditor::cancelRequested, this, [this]() {
        if (m_calendarView) {
            m_calendarView->setFocus();
            m_calendarView->clearGhostPreview();
        }
        if (m_eventEditor) {
            m_eventEditor->clearEditor();
            setInlineEditorActive(false);
        }
    });

    m_previewPanel = new EventPreviewPanel(panel);
    m_previewPanel->setVisible(false);

    layout->addWidget(m_calendarView, 1);
    auto *calendarDeleteShortcut = new QShortcut(QKeySequence::Delete, m_calendarView);
    calendarDeleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(calendarDeleteShortcut, &QShortcut::activated, this, &MainWindow::deleteSelection);

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

    m_cancelPlacementShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_cancelPlacementShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(m_cancelPlacementShortcut, &QShortcut::activated, this, &MainWindow::cancelPendingPlacement);

    auto *undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::performUndo);

    auto *redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoShortcut, &QShortcut::activated, this, &MainWindow::performRedo);

    auto *focusSearchShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    focusSearchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(focusSearchShortcut, &QShortcut::activated, this, [this]() {
        if (!m_todoSearchField) {
            return;
        }
        m_todoSearchField->setFocus(Qt::ShortcutFocusReason);
        m_todoSearchField->selectAll();
    });
}

void MainWindow::saveTodoSplitterState() const
{
    if (!m_todoSplitter) {
        return;
    }
    QSettings settings;
    QVariantList serialized;
    for (int size : m_todoSplitter->sizes()) {
        serialized << size;
    }
    settings.setValue(QStringLiteral("ui/todoSplitterSizes"), serialized);
}

void MainWindow::restoreTodoSplitterState()
{
    if (!m_todoSplitter) {
        return;
    }
    QSettings settings;
    const QVariant value = settings.value(QStringLiteral("ui/todoSplitterSizes"));
    if (!value.isValid()) {
        return;
    }
    const auto list = value.toList();
    if (list.isEmpty()) {
        return;
    }
    QList<int> sizes;
    sizes.reserve(list.size());
    for (const auto &entry : list) {
        sizes << entry.toInt();
    }
    if (sizes.size() == m_todoSplitter->count()) {
        m_todoSplitter->setSizes(sizes);
    }
}

void MainWindow::saveCalendarState() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("calendar/startDate"), m_currentDate);
    settings.setValue(QStringLiteral("calendar/dayOffset"), m_dayOffset);
    settings.setValue(QStringLiteral("calendar/visibleDays"), m_visibleDays);
    const double storedHeight = m_calendarView ? m_calendarView->hourHeight() : m_savedHourHeight;
    settings.setValue(QStringLiteral("calendar/hourHeight"), storedHeight);
    if (m_calendarView) {
        settings.setValue(QStringLiteral("calendar/verticalScroll"), m_calendarView->verticalScrollValue());
    }
}

void MainWindow::restoreCalendarState()
{
    QSettings settings;
    const QDate storedStart = settings.value(QStringLiteral("calendar/startDate")).toDate();
    const bool hasOffsetValue = settings.contains(QStringLiteral("calendar/dayOffset"));
    if (storedStart.isValid()) {
        if (hasOffsetValue) {
            m_currentDate = storedStart;
        } else {
            m_currentDate = alignToWeekStart(storedStart);
        }
    }
    const double storedOffset = settings.value(QStringLiteral("calendar/dayOffset"), 0.0).toDouble();
    if (hasOffsetValue) {
        m_dayOffset = qBound(0.0, storedOffset, 0.999999);
    } else {
        m_dayOffset = 0.0;
    }
    const int storedDays = settings.value(QStringLiteral("calendar/visibleDays"), m_visibleDays).toInt();
    m_visibleDays = qBound(1, storedDays, 31);
    const double storedHeight = settings.value(QStringLiteral("calendar/hourHeight"), m_savedHourHeight).toDouble();
    if (storedHeight > 0.0) {
        m_savedHourHeight = storedHeight;
    }
    m_savedVerticalScroll = settings.value(QStringLiteral("calendar/verticalScroll"), 0).toInt();
}

TodoFilterProxyModel *MainWindow::proxyForView(QListView *view) const
{
    if (view == m_todoPendingView) {
        return m_pendingProxyModel.get();
    }
    if (view == m_todoInProgressView) {
        return m_inProgressProxyModel.get();
    }
    if (view == m_todoDoneView) {
        return m_doneProxyModel.get();
    }
    return nullptr;
}

void MainWindow::updateTodoFilterText(const QString &text)
{
    if (m_pendingProxyModel) {
        m_pendingProxyModel->setFilterText(text);
    }
    if (m_inProgressProxyModel) {
        m_inProgressProxyModel->setFilterText(text);
    }
    if (m_doneProxyModel) {
        m_doneProxyModel->setFilterText(text);
    }
}

void MainWindow::handleTodoStatusDrop(const QList<QUuid> &todoIds, data::TodoStatus status)
{
    bool changed = false;
    for (const auto &id : todoIds) {
        auto todo = m_appContext->todoRepository().findById(id);
        if (!todo.has_value()) {
            continue;
        }
        if (todo->status == status) {
            continue;
        }
        todo->status = status;
        changed |= m_appContext->todoRepository().updateTodo(*todo);
    }
    if (changed) {
        clearAllTodoSelections();
        m_selectedTodo.reset();
        refreshTodos();
        statusBar()->showMessage(tr("TODO-Status aktualisiert"), 1500);
    }
}

void MainWindow::clearOtherTodoSelections(QListView *except)
{
    auto clear = [except](TodoListView *view) {
        if (!view || view == except) {
            return;
        }
        if (auto *selectionModel = view->selectionModel()) {
            QSignalBlocker blocker(selectionModel);
            view->clearSelection();
        }
    };
    clear(m_todoPendingView);
    clear(m_todoInProgressView);
    clear(m_todoDoneView);
}

void MainWindow::clearAllTodoSelections()
{
    auto clear = [](TodoListView *view) {
        if (!view) {
            return;
        }
        if (auto *selectionModel = view->selectionModel()) {
            QSignalBlocker blocker(selectionModel);
            view->clearSelection();
        }
    };
    clear(m_todoPendingView);
    clear(m_todoInProgressView);
    clear(m_todoDoneView);
    m_activeTodoView = nullptr;
}

void MainWindow::goToday()
{
    m_currentDate = alignToWeekStart(QDate::currentDate());
    m_dayOffset = 0.0;
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
    m_dayOffset = 0.0;
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
    m_dayOffset = 0.0;
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
    if (m_pendingProxyModel) {
        m_pendingProxyModel->invalidate();
    }
    if (m_inProgressProxyModel) {
        m_inProgressProxyModel->invalidate();
    }
    if (m_doneProxyModel) {
        m_doneProxyModel->invalidate();
    }
}

void MainWindow::addQuickTodo()
{
    QString initialText;
    if (m_todoSearchField) {
        initialText = m_todoSearchField->text().trimmed();
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Neues TODO"));
    dialog.resize(480, 360);

    auto *layout = new QVBoxLayout(&dialog);
    auto *hint = new QLabel(tr("Eine Zeile pro TODO. Eingeschobene Zeilen beschreiben Details oder \"Ort:\"."), &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *editor = new QPlainTextEdit(&dialog);
    editor->setPlaceholderText(tr("Einkaufen 2h\n\tMilch\n\tOrt: Markt"));
    editor->setPlainText(initialText);
    if (!initialText.isEmpty()) {
        editor->selectAll();
    }
    layout->addWidget(editor, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    layout->addWidget(buttonBox);
    auto *acceptShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Return), &dialog);
    acceptShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(acceptShortcut, &QShortcut::activated, &dialog, [&dialog]() {
        QMetaObject::invokeMethod(&dialog, "accept", Qt::QueuedConnection);
    });
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString content = editor->toPlainText();
    if (content.trimmed().isEmpty()) {
        statusBar()->showMessage(tr("Keine Eingabe für neues TODO"), 2000);
        return;
    }

    const int created = insertTodosFromPlainText(content, data::TodoStatus::Pending);
    if (created <= 0) {
        statusBar()->showMessage(tr("Keine TODOs erkannt"), 2000);
        return;
    }

    statusBar()->showMessage(tr("%1 TODO(s) erstellt").arg(created), 2000);
}

void MainWindow::deleteSelectedTodos()
{
    if (!m_todoViewModel) {
        return;
    }
    const auto todos = selectedTodos();
    if (todos.isEmpty()) {
        return;
    }
    bool removed = false;
    for (const auto &todo : todos) {
        removed |= m_appContext->todoRepository().removeTodo(todo.id);
    }
    if (removed) {
        clearAllTodoSelections();
        m_selectedTodo.reset();
        refreshTodos();
        statusBar()->showMessage(tr("Ausgewählte TODOs gelöscht"), 1500);
    }
}

void MainWindow::handleTodoActivated(const QModelIndex &index)
{
    auto *view = qobject_cast<QListView *>(sender());
    auto *proxy = proxyForView(view);
    if (!proxy || !m_todoViewModel) {
        return;
    }
    const auto sourceIndex = proxy->mapToSource(index);
    const auto *todo = m_todoViewModel->model()->todoAt(sourceIndex);
    if (!todo) {
        return;
    }
    statusBar()->showMessage(tr("TODO \"%1\" (Priorität %2)").arg(todo->title).arg(todo->priority), 2000);
}

void MainWindow::handleTodoDoubleClicked(const QModelIndex &index)
{
    auto *view = qobject_cast<QListView *>(sender());
    auto *proxy = proxyForView(view);
    if (!proxy || !m_todoViewModel) {
        return;
    }
    const auto sourceIndex = proxy->mapToSource(index);
    const auto *todo = m_todoViewModel->model()->todoAt(sourceIndex);
    if (!todo || !m_eventEditor) {
        return;
    }
    m_selectedTodo = *todo;
    m_selectedEvent.reset();
    clearOtherTodoSelections(view);
    if (auto *selection = view->selectionModel()) {
        selection->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    m_eventEditor->setTodo(*todo);
    m_eventEditor->setVisible(true);
    m_eventEditor->focusTitle(true);
    setInlineEditorActive(true);
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
    if (m_selectedEvent.has_value()) {
        if (auto latest = m_appContext->eventRepository().findById(m_selectedEvent->id)) {
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
    const QDate viewEnd = m_currentDate.addDays(m_visibleDays - 1);
    const QDate fetchEnd = viewEnd.addDays(1);
    m_scheduleViewModel->setRange(m_currentDate, fetchEnd);
    m_calendarView->setDateRange(m_currentDate, m_visibleDays);
    m_calendarView->setDayOffset(m_dayOffset);
    if (m_viewInfoLabel) {
        m_viewInfoLabel->setText(tr("%1 - %2 (%3 Tage)")
                                     .arg(m_currentDate.toString(Qt::ISODate),
                                          viewEnd.toString(Qt::ISODate))
                                     .arg(m_visibleDays));
    }
    saveCalendarState();
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
    m_dayOffset = 0.0;
    m_currentDate = m_currentDate.addDays(deltaDays);
    updateCalendarRange();
    refreshCalendar();
    statusBar()->showMessage(tr("Ansicht verschoben: %1")
                                 .arg(m_currentDate.toString(Qt::ISODate)),
                             1200);
}

void MainWindow::scrollVisibleDaysFractional(double deltaDays)
{
    if (qFuzzyIsNull(deltaDays)) {
        return;
    }
    m_dayOffset += deltaDays;
    while (m_dayOffset >= 1.0) {
        m_currentDate = m_currentDate.addDays(1);
        m_dayOffset -= 1.0;
    }
    while (m_dayOffset < 0.0) {
        m_currentDate = m_currentDate.addDays(-1);
        m_dayOffset += 1.0;
    }
    m_dayOffset = qBound(0.0, m_dayOffset, 0.999999);
    updateCalendarRange();
    refreshCalendar();
    QDateTime viewStart(m_currentDate, QTime(0, 0));
    viewStart = viewStart.addSecs(static_cast<qint64>(m_dayOffset * 24.0 * 60.0 * 60.0));
    statusBar()->showMessage(tr("Ansicht verschoben: %1").arg(viewStart.toString(Qt::ISODate)), 800);
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
    m_selectedTodo.reset();
    clearAllTodoSelections();
    if (m_editEventAction) {
        m_editEventAction->setEnabled(true);
    }
    if (m_previewVisible) {
        showPreviewForSelection();
    }
    if (m_eventEditor && m_eventEditor->isVisible()) {
        m_eventEditor->setEvent(event);
    }
    showSelectionHint();
}

void MainWindow::saveEventEdits(const data::CalendarEvent &event)
{
    auto updated = event;
    bool created = false;
    if (m_appContext->eventRepository().findById(event.id).has_value()) {
        m_appContext->eventRepository().updateEvent(updated);
    } else {
        updated = m_appContext->eventRepository().addEvent(updated);
        created = true;
    }
    if (auto latest = m_appContext->eventRepository().findById(updated.id)) {
        updated = *latest;
    }
    m_selectedEvent = updated;
    if (m_eventEditor) {
        m_eventEditor->clearEditor();
        setInlineEditorActive(false);
    }
    if (m_calendarView) {
        m_calendarView->clearGhostPreview();
    }
    refreshCalendar();
    statusBar()->showMessage(created ? tr("Termin erstellt: %1").arg(updated.title)
                                     : tr("Termin gespeichert: %1").arg(updated.title),
                             1500);
    if (m_previewVisible) {
        showPreviewForSelection();
    }
}

void MainWindow::saveTodoEdits(const data::TodoItem &todo)
{
    auto updated = todo;
    const bool ok = m_appContext->todoRepository().updateTodo(updated);
    if (!ok) {
        statusBar()->showMessage(tr("TODO konnte nicht gespeichert werden"), 1500);
        return;
    }
    m_selectedTodo = updated;
    if (m_eventEditor) {
        m_eventEditor->clearEditor();
        setInlineEditorActive(false);
    }
    refreshTodos();
    statusBar()->showMessage(tr("TODO gespeichert: %1").arg(updated.title), 1500);
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
    if (m_selectedEvent && m_selectedEvent->id == id && m_eventEditor && m_eventEditor->isVisible()) {
        m_eventEditor->setEvent(*existing);
    }
    refreshCalendar();
    statusBar()->showMessage(tr("Termin angepasst: %1").arg(existing->title), 1500);
}

void MainWindow::clearSelection()
{
    m_selectedEvent.reset();
    m_selectedTodo.reset();
    clearAllTodoSelections();
    if (m_editEventAction) {
        m_editEventAction->setEnabled(false);
    }
    if (m_eventEditor) {
        m_eventEditor->clearEditor();
        setInlineEditorActive(false);
    }
    if (m_previewPanel) {
        m_previewPanel->clearPreview();
    }
    m_previewVisible = false;
    if (m_calendarView) {
        m_calendarView->clearGhostPreview();
    }
    cancelPendingPlacement();
}

void MainWindow::handleTodoDropped(const QUuid &todoId, const QDateTime &start, bool copy)
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
    const int durationMinutes = todoOpt->durationMinutes > 0 ? todoOpt->durationMinutes : 60;
    event.end = start.addSecs(durationMinutes * 60);
    event.location = todoOpt->location;
    m_appContext->eventRepository().addEvent(event);
    if (!copy) {
        m_appContext->todoRepository().removeTodo(todoId);
    }

    if (!copy) {
        clearAllTodoSelections();
        m_selectedTodo.reset();
        refreshTodos();
    }
    refreshCalendar();
    statusBar()->showMessage(copy ? tr("TODO \"%1\" dupliziert").arg(todoOpt->title)
                                  : tr("TODO \"%1\" eingeplant").arg(todoOpt->title),
                             2000);
}

void MainWindow::handleTodoSelectionChanged(TodoListView *view)
{
    if (!view || !m_todoViewModel) {
        return;
    }
    auto *selectionModel = view->selectionModel();
    if (!selectionModel) {
        return;
    }
    const auto indexes = selectionModel->selectedIndexes();
    if (indexes.isEmpty()) {
        if (m_activeTodoView == view) {
            m_activeTodoView = nullptr;
            bool hadTodo = m_selectedTodo.has_value();
            m_selectedTodo.reset();
            if (hadTodo && m_eventEditor && m_eventEditor->isTodoMode()) {
                m_eventEditor->clearEditor();
                setInlineEditorActive(false);
            }
            if (m_previewVisible && !m_selectedEvent.has_value() && m_previewPanel) {
                m_previewPanel->clearPreview();
                m_previewVisible = false;
            }
        }
        return;
    }
    clearOtherTodoSelections(view);
    m_activeTodoView = view;
    auto *proxy = proxyForView(view);
    if (!proxy) {
        return;
    }
    const auto sourceIndex = proxy->mapToSource(indexes.front());
    if (const auto *todo = m_todoViewModel->model()->todoAt(sourceIndex)) {
        m_selectedTodo = *todo;
        m_selectedEvent.reset();
        if (m_editEventAction) {
            m_editEventAction->setEnabled(false);
        }
        if (m_calendarView) {
            m_calendarView->clearExternalSelection();
        }
        cancelPendingPlacement();
        if (m_previewVisible) {
            showPreviewForSelection();
        }
        if (m_eventEditor && m_eventEditor->isVisible()) {
            m_eventEditor->setTodo(*m_selectedTodo);
        }
    }
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

void MainWindow::handleEventDroppedToTodo(const data::CalendarEvent &event, data::TodoStatus status)
{
    data::TodoItem todo;
    todo.title = event.title.isEmpty() ? tr("Termin %1").arg(QLocale().toString(event.start, QLocale::ShortFormat)) : event.title;
    todo.description = event.description;
    todo.location = event.location;
    todo.dueDate = event.start;
    todo.priority = 0;
    todo.status = status;
    todo.scheduled = false;
    const qint64 durationMinutes = qMax<qint64>(15, event.start.secsTo(event.end) / 60);
    todo.durationMinutes = static_cast<int>(durationMinutes);

    m_appContext->todoRepository().addTodo(todo);
    m_appContext->eventRepository().removeEvent(event.id);
    if (m_selectedEvent && m_selectedEvent->id == event.id) {
        clearSelection();
    }
    refreshTodos();
    refreshCalendar();
    clearTodoHoverGhosts();
    statusBar()->showMessage(tr("Termin \"%1\" als TODO erfasst").arg(todo.title), 2000);
}

void MainWindow::handleTodoHoverPreview(data::TodoStatus status, const data::CalendarEvent &event)
{
    clearTodoHoverGhosts();
    auto *view = todoViewForStatus(status);
    if (!view) {
        return;
    }
    const QString title = event.title.isEmpty() ? tr("(Ohne Titel)") : event.title;
    const qint64 durationMinutes = qMax<qint64>(15, event.start.secsTo(event.end) / 60);
    const int hours = static_cast<int>(durationMinutes / 60);
    const int minutes = static_cast<int>(durationMinutes % 60);
    QString durationText;
    if (hours > 0 && minutes > 0) {
        durationText = tr("%1h %2m").arg(hours).arg(minutes);
    } else if (hours > 0) {
        durationText = tr("%1h").arg(hours);
    } else if (minutes > 0) {
        durationText = tr("%1m").arg(minutes);
    }
    const QString label = durationText.isEmpty() ? title : tr("%1 (%2)").arg(title, durationText);
    view->showGhostPreview(label);
}

void MainWindow::handleTodoHoverCleared()
{
    clearTodoHoverGhosts();
}

void MainWindow::handlePlacementConfirmed(const QDateTime &start)
{
    if (!m_pendingPlacement) {
        return;
    }
    QDateTime snapped = snapToQuarterHour(start);
    QDateTime anchored = applyPlacementAnchor(snapped, m_pendingPlacementDuration);
    pasteClipboardAt(anchored);
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
        const QDateTime snapped = snapToQuarterHour(dt);
        const QDateTime anchored = applyPlacementAnchor(snapped, m_pendingPlacementDuration);
        m_calendarView->updatePlacementPreview(anchored);
    }
}

void MainWindow::handleEventCreationRequest(const QDateTime &start, const QDateTime &end)
{
    cancelPendingPlacement();
    m_selectedEvent.reset();
    m_selectedTodo.reset();
    if (m_editEventAction) {
        m_editEventAction->setEnabled(false);
    }
    clearAllTodoSelections();
    if (m_previewPanel) {
        m_previewPanel->clearPreview();
    }
    m_previewVisible = false;

    data::CalendarEvent event;
    event.title = tr("Neuer Termin");
    event.start = start;
    event.end = end;
    if (m_eventEditor) {
        m_eventEditor->setEvent(event);
        m_eventEditor->setVisible(true);
        m_eventEditor->focusTitle(true);
        setInlineEditorActive(true);
    }
    statusBar()->showMessage(tr("Neuen Termin festgelegt: %1 - %2")
                                 .arg(start.toString(QStringLiteral("hh:mm")),
                                      end.toString(QStringLiteral("hh:mm"))),
                             2000);
}

void MainWindow::handleCalendarSelectionCleared()
{
    if (m_eventEditor && m_eventEditor->isVisible()) {
        m_eventEditor->commitChanges();
    }
    clearSelection();
}

void MainWindow::handleInlineEditRequest(const data::CalendarEvent &event)
{
    m_selectedEvent = event;
    m_selectedTodo.reset();
    clearAllTodoSelections();
    if (m_eventEditor) {
        m_eventEditor->setEvent(event);
        m_eventEditor->focusTitle(true);
        setInlineEditorActive(true);
    }
}

void MainWindow::openInlineEditor()
{
    if (!m_eventEditor) {
        return;
    }
    cancelPendingPlacement();
    m_previewVisible = false;
    if (m_previewPanel) {
        m_previewPanel->clearPreview();
    }
    if (m_selectedEvent.has_value()) {
        m_eventEditor->setEvent(*m_selectedEvent);
        m_eventEditor->setVisible(true);
        m_eventEditor->focusTitle(true);
        setInlineEditorActive(true);
        return;
    }
    if (m_selectedTodo.has_value()) {
        m_eventEditor->setTodo(*m_selectedTodo);
        m_eventEditor->setVisible(true);
        m_eventEditor->focusTitle(true);
        setInlineEditorActive(true);
    }
}

void MainWindow::openDetailDialog()
{
    if (!m_selectedEvent.has_value()) {
        return;
    }
    if (!m_eventDetailDialog) {
        m_eventDetailDialog = std::make_unique<EventDetailDialog>(this);
    }
    m_eventDetailDialog->setEvent(*m_selectedEvent);
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

void MainWindow::openSettingsDialog()
{
    if (!m_settingsDialog) {
        m_settingsDialog = std::make_unique<SettingsDialog>(this);
    }
    m_settingsDialog->exec();
}

void MainWindow::togglePreviewPanel()
{
    if (!m_previewPanel) {
        return;
    }
    const bool hasEvent = m_selectedEvent.has_value();
    const bool hasTodo = m_selectedTodo.has_value();
    if (!hasEvent && !hasTodo) {
        return;
    }
    cancelPendingPlacement();
    if (m_eventEditor) {
        m_eventEditor->clearEditor();
        setInlineEditorActive(false);
    }
    m_previewVisible = !m_previewVisible;
    if (m_previewVisible) {
        showPreviewForSelection();
    } else {
        m_previewPanel->clearPreview();
    }
}

void MainWindow::showPreviewForSelection()
{
    if (!m_previewPanel) {
        return;
    }
    if (m_selectedEvent.has_value()) {
        m_previewPanel->setEvent(*m_selectedEvent);
        m_previewVisible = true;
        return;
    }
    if (m_selectedTodo.has_value()) {
        m_previewPanel->setTodo(*m_selectedTodo);
        m_previewVisible = true;
        return;
    }
    m_previewPanel->clearPreview();
    m_previewVisible = false;
}

void MainWindow::showSelectionHint()
{
    if (m_shortcutLabel) {
        m_shortcutLabel->setText(tr("Shortcuts: ⏎ Details • E Inline • Ctrl+C Copy • Ctrl+V Paste • Ctrl+D Duplizieren • Del Löschen • Space Info"));
    }
}

void MainWindow::copySelection()
{
    if (m_selectedEvent.has_value()) {
        m_clipboardEvents.clear();
        m_clipboardEvents.push_back(*m_selectedEvent);
        statusBar()->showMessage(tr("Termin kopiert: %1").arg(m_selectedEvent->title), 1500);
        return;
    }
    auto todos = selectedTodos();
    if (!todos.isEmpty()) {
        copyTodosAsPlainText(todos);
    }
}

void MainWindow::copyTodosAsPlainText(const QList<data::TodoItem> &todos)
{
    if (todos.isEmpty()) {
        statusBar()->showMessage(tr("Keine TODOs ausgewählt"), 1500);
        return;
    }
    const QString plain = todosToPlainText(todos);
    if (plain.isEmpty()) {
        statusBar()->showMessage(tr("Keine Daten zum Kopieren"), 1500);
        return;
    }
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(plain, QClipboard::Clipboard);
    }
    statusBar()->showMessage(tr("%1 TODO(s) kopiert").arg(todos.size()), 2000);
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
    QDateTime anchored = applyPlacementAnchor(target, m_pendingPlacementDuration);
    m_calendarView->beginPlacementPreview(m_pendingPlacementDuration, m_pendingPlacementLabel, anchored);
    statusBar()->showMessage(tr("Klick zum Einfügen – Esc bricht ab"), 4000);
}

void MainWindow::duplicateSelection()
{
    if (!m_selectedEvent.has_value()) {
        return;
    }
    m_clipboardEvents = { *m_selectedEvent };
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

QList<data::TodoItem> MainWindow::selectedTodos() const
{
    QList<data::TodoItem> items;
    auto collect = [&](TodoListView *view) {
        if (!view || !m_todoViewModel) {
            return;
        }
        auto *selection = view->selectionModel();
        auto *proxy = proxyForView(view);
        if (!selection || !proxy) {
            return;
        }
        auto indexes = selection->selectedIndexes();
        if (indexes.isEmpty()) {
            return;
        }
        std::sort(indexes.begin(), indexes.end(), [](const QModelIndex &lhs, const QModelIndex &rhs) {
            return lhs.row() < rhs.row();
        });
        auto *model = m_todoViewModel->model();
        if (!model) {
            return;
        }
        for (const auto &index : indexes) {
            const QModelIndex sourceIndex = proxy->mapToSource(index);
            if (const auto *todo = model->todoAt(sourceIndex)) {
                items.append(*todo);
            }
        }
    };
    collect(m_todoPendingView);
    collect(m_todoInProgressView);
    collect(m_todoDoneView);
    return items;
}

QString MainWindow::todosToPlainText(const QList<data::TodoItem> &todos) const
{
    QStringList lines;
    for (const auto &todo : todos) {
        QString line = todo.title.isEmpty() ? tr("(Ohne Titel)") : todo.title;
        const QString durationToken = durationTokenForMinutes(todo.durationMinutes);
        if (!durationToken.isEmpty()) {
            line += QStringLiteral(" %1").arg(durationToken);
        }
        lines << line;
        if (!todo.description.isEmpty()) {
            const QStringList descLines = todo.description.split('\n');
            for (const auto &descLine : descLines) {
                const QString trimmed = descLine.trimmed();
                if (!trimmed.isEmpty()) {
                    lines << QStringLiteral("\t%1").arg(trimmed);
                }
            }
        }
        if (!todo.location.trimmed().isEmpty()) {
            lines << QStringLiteral("\tOrt: %1").arg(todo.location.trimmed());
        }
    }
    return lines.join(QLatin1Char('\n'));
}

void MainWindow::pasteTodosFromPlainText(data::TodoStatus status)
{
    auto *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        statusBar()->showMessage(tr("Zwischenablage nicht verfügbar"), 2000);
        return;
    }
    const QString content = clipboard->text(QClipboard::Clipboard);
    const int count = insertTodosFromPlainText(content, status);
    if (count <= 0) {
        statusBar()->showMessage(tr("Keine TODOs erkannt"), 2000);
        return;
    }
    statusBar()->showMessage(tr("%1 TODO(s) eingefügt").arg(count), 2000);
}

int MainWindow::insertTodosFromPlainText(const QString &text, data::TodoStatus status)
{
    if (!m_appContext) {
        return 0;
    }
    const auto parsed = parsePlainTextTodos(text);
    if (parsed.isEmpty()) {
        return 0;
    }
    std::vector<data::TodoItem> templates;
    templates.reserve(static_cast<std::size_t>(parsed.size()));
    for (const auto &entry : parsed) {
        data::TodoItem todo;
        todo.title = entry.title;
        todo.description = entry.description;
        todo.location = entry.location;
        todo.status = status;
        todo.durationMinutes = entry.durationMinutes;
        todo.scheduled = false;
        templates.push_back(std::move(todo));
    }
    auto command = std::make_unique<PlainTextInsertCommand>(m_appContext->todoRepository(), std::move(templates));
    m_appContext->undoStack().push(std::move(command));
    refreshTodos();
    return parsed.size();
}

void MainWindow::performUndo()
{
    if (!m_appContext) {
        return;
    }
    auto &stack = m_appContext->undoStack();
    if (!stack.canUndo()) {
        statusBar()->showMessage(tr("Nichts zum Rückgängig machen"), 1500);
        return;
    }
    stack.undo();
    refreshTodos();
    refreshCalendar();
    statusBar()->showMessage(tr("Aktion rückgängig gemacht"), 2000);
}

void MainWindow::performRedo()
{
    if (!m_appContext) {
        return;
    }
    auto &stack = m_appContext->undoStack();
    if (!stack.canRedo()) {
        statusBar()->showMessage(tr("Nichts zum Wiederholen"), 1500);
        return;
    }
    stack.redo();
    refreshTodos();
    refreshCalendar();
    statusBar()->showMessage(tr("Aktion wiederholt"), 2000);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_todoSearchField && event) {
        if (event->type() == QEvent::ShortcutOverride) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                if (!m_todoSearchField->text().isEmpty()) {
                    m_todoSearchField->clear();
                }
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                if (!m_todoSearchField->text().isEmpty()) {
                    m_todoSearchField->clear();
                }
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
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
    if (m_selectedEvent.has_value()) {
        if (m_eventEditor) {
            m_eventEditor->clearEditor();
            setInlineEditorActive(false);
        }
        cancelPendingPlacement();
        const bool removed = m_appContext->eventRepository().removeEvent(m_selectedEvent->id);
        if (removed) {
            statusBar()->showMessage(tr("Termin gelöscht"), 2000);
            m_selectedEvent.reset();
            if (m_previewPanel) {
                m_previewPanel->clearPreview();
            }
            m_previewVisible = false;
            refreshCalendar();
        }
        return;
    }
    deleteSelectedTodos();
}

void MainWindow::clearTodoHoverGhosts()
{
    if (m_todoPendingView) {
        m_todoPendingView->clearGhostPreview();
    }
    if (m_todoInProgressView) {
        m_todoInProgressView->clearGhostPreview();
    }
    if (m_todoDoneView) {
        m_todoDoneView->clearGhostPreview();
    }
}

TodoListView *MainWindow::todoViewForStatus(data::TodoStatus status) const
{
    switch (status) {
    case data::TodoStatus::Pending:
        return m_todoPendingView;
    case data::TodoStatus::InProgress:
        return m_todoInProgressView;
    case data::TodoStatus::Completed:
        return m_todoDoneView;
    }
    return nullptr;
}

void MainWindow::setInlineEditorActive(bool active)
{
    if (m_cancelPlacementShortcut) {
        m_cancelPlacementShortcut->setEnabled(!active);
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
