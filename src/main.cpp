#include <QApplication>

#include "calendar/ui/MainWindow.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    calendar::ui::MainWindow mainWindow;
    mainWindow.showMaximized();

    return app.exec();
}
