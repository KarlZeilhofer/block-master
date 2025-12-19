#include "calendar/ui/MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QCalendarWidget>
#include <QDate>
#include <QFrame>
#include <QLabel>
#include <QListView>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include "calendar/core/AppContext.hpp"

namespace calendar {
namespace ui {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_appContext(std::make_unique<core::AppContext>())
{
    setupUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(tr("Calendar App (Prototype)"));
    resize(1200, 800);

    auto *toolbarWidget = createNavigationBar();
    addToolBar(Qt::TopToolBarArea, qobject_cast<QToolBar *>(toolbarWidget));

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_todoPanel = createTodoPanel();
    m_calendarPanel = createCalendarPlaceholder();

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
    statusBar()->showMessage(tr("Bereit"));
}

QWidget *MainWindow::createNavigationBar()
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

    auto *viewLabel = new QLabel(tr("Ansicht: Tag (Stub)"), toolbar);
    toolbar->addWidget(viewLabel);

    setupShortcuts(toolbar);
    return toolbar;
}

QWidget *MainWindow::createTodoPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    auto *title = new QLabel(tr("TODOs"), panel);
    title->setObjectName(QStringLiteral("todoTitle"));
    auto *todoList = new QListView(panel);
    todoList->setObjectName(QStringLiteral("todoList"));
    todoList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    todoList->setDragEnabled(true);
    todoList->setToolTip(tr("TODO-Liste (Drag & Drop in Kalender)"));

    layout->addWidget(title);
    layout->addWidget(todoList, 1);
    panel->setMinimumWidth(280);
    panel->setMaximumWidth(450);

    return panel;
}

QWidget *MainWindow::createCalendarPlaceholder()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    auto *header = new QLabel(tr("Kalenderansicht (Stub)"), panel);
    auto *calendarPreview = new QLabel(tr("Hier folgen Timeline/Zoom-Widgets in späteren Schritten"), panel);
    calendarPreview->setAlignment(Qt::AlignCenter);
    calendarPreview->setFrameShape(QFrame::StyledPanel);
    calendarPreview->setMinimumHeight(600);

    layout->addWidget(header);
    layout->addWidget(calendarPreview, 1);
    return panel;
}

void MainWindow::setupShortcuts(QToolBar *toolbar)
{
    auto *newTodoAction = toolbar->addAction(tr("Neues TODO"));
    newTodoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));

    auto *newEventAction = toolbar->addAction(tr("Neuer Termin"));
    newEventAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
}

void MainWindow::goToday()
{
    statusBar()->showMessage(tr("Heute ausgewählt: %1").arg(QDate::currentDate().toString(Qt::ISODate)), 2000);
}

void MainWindow::navigateForward()
{
    statusBar()->showMessage(tr("Weiter (Stub)"), 1500);
}

void MainWindow::navigateBackward()
{
    statusBar()->showMessage(tr("Zurück (Stub)"), 1500);
}

} // namespace ui
} // namespace calendar
