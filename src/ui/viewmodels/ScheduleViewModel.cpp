#include "calendar/ui/viewmodels/ScheduleViewModel.hpp"

#include "calendar/data/EventRepository.hpp"

namespace calendar {
namespace ui {

ScheduleViewModel::ScheduleViewModel(data::EventRepository &repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
}

void ScheduleViewModel::setRange(const QDate &start, const QDate &end)
{
    if (!start.isValid() || !end.isValid()) {
        return;
    }
    m_start = start;
    m_end = end;
}

void ScheduleViewModel::refresh()
{
    if (!m_start.isValid() || !m_end.isValid()) {
        return;
    }
    m_events = m_repository.fetchEvents(m_start, m_end);
    emit eventsChanged(m_events);
}

const std::vector<data::CalendarEvent> &ScheduleViewModel::events() const
{
    return m_events;
}

} // namespace ui
} // namespace calendar
