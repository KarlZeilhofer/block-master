#pragma once

#include <QDate>
#include <QObject>
#include <vector>

#include "calendar/data/Event.hpp"

namespace calendar {
namespace data {
class EventRepository;
}

namespace ui {

class ScheduleViewModel : public QObject
{
    Q_OBJECT

public:
    ScheduleViewModel(data::EventRepository &repository, QObject *parent = nullptr);

    void setRange(const QDate &start, const QDate &end);
    void refresh();
    const std::vector<data::CalendarEvent> &events() const;

signals:
    void eventsChanged(const std::vector<data::CalendarEvent> &events);

private:
    data::EventRepository &m_repository;
    QDate m_start;
    QDate m_end;
    std::vector<data::CalendarEvent> m_events;
};

} // namespace ui
} // namespace calendar
