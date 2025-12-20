#include "calendar/data/FileEventRepository.hpp"

#include <algorithm>

namespace calendar {
namespace data {

FileEventRepository::FileEventRepository(std::shared_ptr<FileCalendarStorage> storage)
    : m_storage(std::move(storage))
{
}

std::vector<CalendarEvent> FileEventRepository::fetchEvents(const QDate &from, const QDate &to) const
{
    std::vector<CalendarEvent> result;
    if (!m_storage) {
        return result;
    }

    const auto &events = m_storage->events();
    result.reserve(static_cast<size_t>(events.size()));
    for (auto it = events.constBegin(); it != events.constEnd(); ++it) {
        const auto &event = it.value();
        if (event.end.date() < from || event.start.date() > to) {
            continue;
        }
        result.push_back(event);
    }
    std::sort(result.begin(), result.end(), [](const CalendarEvent &lhs, const CalendarEvent &rhs) {
        if (lhs.start == rhs.start) {
            return lhs.end < rhs.end;
        }
        return lhs.start < rhs.start;
    });
    return result;
}

std::optional<CalendarEvent> FileEventRepository::findById(const QUuid &id) const
{
    if (!m_storage) {
        return std::nullopt;
    }
    const auto &events = m_storage->events();
    if (events.contains(id)) {
        return events.value(id);
    }
    return std::nullopt;
}

CalendarEvent FileEventRepository::addEvent(CalendarEvent event)
{
    if (!m_storage) {
        return event;
    }
    return m_storage->addOrUpdateEvent(std::move(event));
}

bool FileEventRepository::updateEvent(const CalendarEvent &event)
{
    if (!m_storage) {
        return false;
    }
    const auto &events = m_storage->events();
    if (!events.contains(event.id)) {
        return false;
    }
    m_storage->addOrUpdateEvent(event);
    return true;
}

bool FileEventRepository::removeEvent(const QUuid &id)
{
    if (!m_storage) {
        return false;
    }
    return m_storage->removeEvent(id);
}

} // namespace data
} // namespace calendar

