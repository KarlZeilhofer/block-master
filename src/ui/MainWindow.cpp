#include "calendar/ui/MainWindow.hpp"

#include <QApplication>
#include <QLabel>
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

    auto *toolbar = addToolBar(tr("Navigation"));
    toolbar->setMovable(false);

    auto *centralWidget = new QWidget(this);
    auto *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *placeholder = new QLabel(tr("Kalenderansicht folgt in späteren Schritten"), centralWidget);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);

    setCentralWidget(centralWidget);
    statusBar()->showMessage(tr("Bereit"));
}

} // namespace ui
} // namespace calendar
