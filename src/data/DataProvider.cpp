#include "calendar/data/DataProvider.hpp"

#include "calendar/data/FileCalendarStorage.hpp"
#include "calendar/data/FileEventRepository.hpp"
#include "calendar/data/FileTodoRepository.hpp"
#include "calendar/data/TodoRepository.hpp"

#include <QDir>
#include <QStandardPaths>

namespace calendar {
namespace data {

DataProvider::DataProvider()
{
    QString storageFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (storageFolder.isEmpty()) {
        storageFolder = QDir::homePath() + QStringLiteral("/.local/share/block-master");
    }
    QDir dir(storageFolder);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    const QString filePath = dir.filePath(QStringLiteral("default.ics"));

    m_calendarStorage = std::make_shared<FileCalendarStorage>(filePath);
    m_todoRepository = std::make_unique<FileTodoRepository>(m_calendarStorage);
    m_eventRepository = std::make_unique<FileEventRepository>(m_calendarStorage);

}

DataProvider::~DataProvider() = default;

TodoRepository &DataProvider::todoRepository()
{
    return *m_todoRepository;
}

EventRepository &DataProvider::eventRepository()
{
    return *m_eventRepository;
}

} // namespace data
} // namespace calendar
