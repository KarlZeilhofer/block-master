#include "calendar/data/DataProvider.hpp"

#include "calendar/data/FileCalendarStorage.hpp"
#include "calendar/data/FileEventRepository.hpp"
#include "calendar/data/FileTodoRepository.hpp"
#include "calendar/data/Todo.hpp"
#include "calendar/data/TodoRepository.hpp"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QTime>
#include <QObject>
#include <QStandardPaths>

namespace calendar {
namespace data {

DataProvider::DataProvider()
{
    QString storageFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (storageFolder.isEmpty()) {
        storageFolder = QDir::homePath() + QStringLiteral("/.local/share/task-master");
    }
    QDir dir(storageFolder);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    const QString filePath = dir.filePath(QStringLiteral("default.ics"));

    m_calendarStorage = std::make_shared<FileCalendarStorage>(filePath);
    m_todoRepository = std::make_unique<FileTodoRepository>(m_calendarStorage);
    m_eventRepository = std::make_unique<FileEventRepository>(m_calendarStorage);

    seedDemoData();
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

void DataProvider::seedDemoData()
{
    if (!m_todoRepository) {
        return;
    }
    if (m_todoRepository->fetchTodos().empty()) {
        TodoItem quickWin;
        quickWin.title = QObject::tr("Review Kalender-Spezifikation");
        quickWin.description = QObject::tr("Spezifikation validieren und Tasks erstellen");
        quickWin.dueDate = QDateTime::currentDateTime().addDays(1);
        quickWin.priority = 2;

        TodoItem dragDrop;
        dragDrop.title = QObject::tr("Drag & Drop testen");
        dragDrop.description = QObject::tr("TODOs in Kalender ziehen und Dauer festlegen");
        dragDrop.dueDate = QDateTime::currentDateTime().addDays(2);
        dragDrop.priority = 1;
        dragDrop.status = TodoStatus::InProgress;

        TodoItem polish;
        polish.title = QObject::tr("Shortcut-Übersicht");
        polish.description = QObject::tr("Liste der Kürzel im Wiki dokumentieren");
        polish.dueDate = QDateTime::currentDateTime().addDays(3);
        polish.priority = 3;
        polish.status = TodoStatus::Pending;

        m_todoRepository->addTodo(std::move(quickWin));
        m_todoRepository->addTodo(std::move(dragDrop));
        m_todoRepository->addTodo(std::move(polish));
    }

    if (m_eventRepository && m_eventRepository->fetchEvents(QDate::currentDate().addDays(-1),
                                                           QDate::currentDate().addDays(7)).empty()) {
        CalendarEvent standup;
        standup.title = QObject::tr("Daily Standup");
        standup.start = QDateTime(QDate::currentDate(), QTime(9, 0));
        standup.end = standup.start.addSecs(30 * 60);
        standup.location = QObject::tr("Huddle Room");

        CalendarEvent planning;
        planning.title = QObject::tr("Planungsmeeting");
        planning.start = QDateTime(QDate::currentDate().addDays(1), QTime(14, 0));
        planning.end = planning.start.addSecs(90 * 60);
        planning.location = QObject::tr("Konferenzraum A");

        CalendarEvent design;
        design.title = QObject::tr("Design Sprint");
        design.start = QDateTime(QDate::currentDate().addDays(2), QTime(10, 30));
        design.end = design.start.addSecs(120 * 60);
        design.location = QObject::tr("Remote");

        m_eventRepository->addEvent(std::move(standup));
        m_eventRepository->addEvent(std::move(planning));
        m_eventRepository->addEvent(std::move(design));
    }
}

} // namespace data
} // namespace calendar
