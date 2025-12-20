#pragma once

#include <QHash>
#include <QDateTime>
#include <QString>
#include <QUuid>
#include <memory>

#include "calendar/data/Event.hpp"
#include "calendar/data/Todo.hpp"

namespace calendar {
namespace data {

class FileCalendarStorage
{
public:
    explicit FileCalendarStorage(QString filePath);
    ~FileCalendarStorage() = default;

    const QHash<QUuid, CalendarEvent> &events() const;
    const QHash<QUuid, TodoItem> &todos() const;

    CalendarEvent addOrUpdateEvent(CalendarEvent event);
    bool removeEvent(const QUuid &id);

    TodoItem addOrUpdateTodo(TodoItem todo);
    bool removeTodo(const QUuid &id);

private:
    void load();
    void save() const;

    static QString encodeText(const QString &text);
    static QString decodeText(const QString &text);
    static QString formatDateTime(const QDateTime &dt);
    static QDateTime parseDateTime(const QString &value);
    static QString statusToString(TodoStatus status);
    static TodoStatus statusFromString(const QString &value);

    QString m_filePath;
    QHash<QUuid, CalendarEvent> m_events;
    QHash<QUuid, TodoItem> m_todos;
};

} // namespace data
} // namespace calendar

