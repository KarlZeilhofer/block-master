#include "calendar/data/DataProvider.hpp"

#include "calendar/data/InMemoryEventRepository.hpp"
#include "calendar/data/InMemoryTodoRepository.hpp"
#include "calendar/data/Todo.hpp"
#include "calendar/data/TodoRepository.hpp"

#include <QDateTime>
#include <QObject>

namespace calendar {
namespace data {

DataProvider::DataProvider()
    : m_todoRepository(std::make_unique<InMemoryTodoRepository>())
    , m_eventRepository(std::make_unique<InMemoryEventRepository>())
{
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
    if (!m_todoRepository->fetchTodos().empty()) {
        return;
    }

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

} // namespace data
} // namespace calendar
