#pragma once

#include <QMultiHash>

#include "calendar/data/EventRepository.hpp"

namespace calendar {
namespace data {

class InMemoryEventRepository : public EventRepository
{
public:
    InMemoryEventRepository();
    ~InMemoryEventRepository() override;

    std::vector<CalendarEvent> fetchEvents(const QDate &from, const QDate &to) const override;
    std::optional<CalendarEvent> findById(const QUuid &id) const override;
    CalendarEvent addEvent(CalendarEvent event) override;
    bool updateEvent(const CalendarEvent &event) override;
    bool removeEvent(const QUuid &id) override;

private:
    QHash<QUuid, CalendarEvent> m_events;
};

} // namespace data
} // namespace calendar
