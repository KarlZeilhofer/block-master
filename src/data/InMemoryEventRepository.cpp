#include "calendar/data/InMemoryEventRepository.hpp"

namespace calendar {
namespace data {

InMemoryEventRepository::InMemoryEventRepository() = default;
InMemoryEventRepository::~InMemoryEventRepository() = default;

std::vector<CalendarEvent> InMemoryEventRepository::fetchEvents(const QDate &from, const QDate &to) const
{
    std::vector<CalendarEvent> events;
    for (const auto &event : m_events) {
        if (event.end.date() < from || event.start.date() > to) {
            continue;
        }
        events.push_back(event);
    }
    return events;
}

std::optional<CalendarEvent> InMemoryEventRepository::findById(const QUuid &id) const
{
    if (m_events.contains(id)) {
        return m_events.value(id);
    }
    return std::nullopt;
}

CalendarEvent InMemoryEventRepository::addEvent(CalendarEvent event)
{
    if (event.id.isNull()) {
        event.id = QUuid::createUuid();
    }
    if (!event.end.isValid() || event.end <= event.start) {
        event.end = event.start.addSecs(30 * 60);
    }
    m_events.insert(event.id, event);
    return event;
}

bool InMemoryEventRepository::updateEvent(const CalendarEvent &event)
{
    if (!m_events.contains(event.id)) {
        return false;
    }
    m_events.insert(event.id, event);
    return true;
}

bool InMemoryEventRepository::removeEvent(const QUuid &id)
{
    return m_events.remove(id) > 0;
}

} // namespace data
} // namespace calendar
