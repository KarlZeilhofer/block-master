#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QUuid>

namespace calendar {
namespace data {

struct CalendarEvent
{
    QUuid id = QUuid::createUuid();
    QString title;
    QString description;
    QDateTime start;
    QDateTime end;
    bool allDay = false;
    QString location;
    QStringList categories;
};

} // namespace data
} // namespace calendar
