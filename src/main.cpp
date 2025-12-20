#include <QApplication>
#include <QCoreApplication>

#include "calendar/ui/MainWindow.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("Zellhoff"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("zellhoff.at"));
    QCoreApplication::setApplicationName(QStringLiteral("Block Master"));

    QApplication app(argc, argv);

    calendar::ui::MainWindow mainWindow;
    mainWindow.setWindowTitle(QObject::tr("Block Master"));
    mainWindow.showMaximized();

    return app.exec();
}
