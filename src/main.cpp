#include <QApplication>
#include <QCoreApplication>
#include <QSettings>

#include "build_number.hpp"

#include "calendar/ui/MainWindow.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("Zellhoff"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("zellhoff.at"));
    QCoreApplication::setApplicationName(QStringLiteral("Block Master"));

    QApplication app(argc, argv);

    calendar::ui::MainWindow mainWindow;
    mainWindow.setWindowTitle(QObject::tr("Block Master - build %1").arg(kBuildNumber));
    mainWindow.showMaximized();

    return app.exec();
}
