#pragma once

#include <optional>
#include <vector>

#include "calendar/data/Event.hpp"

namespace calendar {
namespace data {

class EventRepository
{
public:
    virtual ~EventRepository() = default;

    virtual std::vector<CalendarEvent> fetchEvents(const QDate &from, const QDate &to) const = 0;
    virtual std::optional<CalendarEvent> findById(const QUuid &id) const = 0;
    virtual CalendarEvent addEvent(CalendarEvent event) = 0;
    virtual bool updateEvent(const CalendarEvent &event) = 0;
    virtual bool removeEvent(const QUuid &id) = 0;
};

} // namespace data
} // namespace calendar
