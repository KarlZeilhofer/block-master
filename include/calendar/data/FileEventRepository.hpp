#pragma once

#include "calendar/data/EventRepository.hpp"
#include "calendar/data/FileCalendarStorage.hpp"

#include <memory>

namespace calendar {
namespace data {

class FileEventRepository : public EventRepository
{
public:
    explicit FileEventRepository(std::shared_ptr<FileCalendarStorage> storage);
    ~FileEventRepository() override = default;

    std::vector<CalendarEvent> fetchEvents(const QDate &from, const QDate &to) const override;
    std::optional<CalendarEvent> findById(const QUuid &id) const override;
    CalendarEvent addEvent(CalendarEvent event) override;
    bool updateEvent(const CalendarEvent &event) override;
    bool removeEvent(const QUuid &id) override;

private:
    std::shared_ptr<FileCalendarStorage> m_storage;
};

} // namespace data
} // namespace calendar

