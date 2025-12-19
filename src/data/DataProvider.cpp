#include "calendar/data/DataProvider.hpp"

#include "calendar/data/InMemoryEventRepository.hpp"
#include "calendar/data/InMemoryTodoRepository.hpp"

namespace calendar {
namespace data {

DataProvider::DataProvider()
    : m_todoRepository(std::make_unique<InMemoryTodoRepository>())
    , m_eventRepository(std::make_unique<InMemoryEventRepository>())
{
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
