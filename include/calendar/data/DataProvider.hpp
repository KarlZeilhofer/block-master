#pragma once

#include <memory>
#include <QString>

namespace calendar {
namespace data {

class TodoRepository;
class EventRepository;
class FileCalendarStorage;

class DataProvider
{
public:
    DataProvider();
    ~DataProvider();

    TodoRepository &todoRepository();
    EventRepository &eventRepository();

private:
    std::shared_ptr<FileCalendarStorage> m_calendarStorage;
    std::unique_ptr<TodoRepository> m_todoRepository;
    std::unique_ptr<EventRepository> m_eventRepository;
};

} // namespace data
} // namespace calendar
