#pragma once

#include <memory>

namespace calendar {
namespace data {

class TodoRepository;
class EventRepository;

class DataProvider
{
public:
    DataProvider();
    ~DataProvider();

    TodoRepository &todoRepository();
    EventRepository &eventRepository();

private:
    std::unique_ptr<TodoRepository> m_todoRepository;
    std::unique_ptr<EventRepository> m_eventRepository;
    void seedDemoData();
};

} // namespace data
} // namespace calendar
