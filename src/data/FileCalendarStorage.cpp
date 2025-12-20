#include "calendar/data/FileCalendarStorage.hpp"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QTime>
#include <algorithm>

namespace calendar {
namespace data {

namespace {
constexpr auto DATE_FORMAT = "yyyyMMdd";
constexpr auto DATE_TIME_FORMAT = "yyyyMMdd'T'hhmmss'Z'";

QString prepareUid(const QUuid &id)
{
    return id.toString(QUuid::WithoutBraces);
}

QUuid parseUid(const QString &value)
{
    const QString withBraces = QStringLiteral("{%1}").arg(value);
    QUuid id(withBraces);
    if (id.isNull()) {
        return QUuid::createUuid();
    }
    return id;
}

QStringList splitList(const QString &value)
{
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    QStringList cleaned;
    cleaned.reserve(parts.size());
    for (const QString &part : parts) {
        cleaned << part.trimmed();
    }
    return cleaned;
}
} // namespace

FileCalendarStorage::FileCalendarStorage(QString filePath)
    : m_filePath(std::move(filePath))
{
    load();
}

const QHash<QUuid, CalendarEvent> &FileCalendarStorage::events() const
{
    return m_events;
}

const QHash<QUuid, TodoItem> &FileCalendarStorage::todos() const
{
    return m_todos;
}

CalendarEvent FileCalendarStorage::addOrUpdateEvent(CalendarEvent event)
{
    if (event.id.isNull()) {
        event.id = QUuid::createUuid();
    }
    if (!event.end.isValid() || event.end <= event.start) {
        event.end = event.start.addSecs(30 * 60);
    }
    m_events.insert(event.id, event);
    save();
    return event;
}

bool FileCalendarStorage::removeEvent(const QUuid &id)
{
    if (m_events.remove(id) > 0) {
        save();
        return true;
    }
    return false;
}

TodoItem FileCalendarStorage::addOrUpdateTodo(TodoItem todo)
{
    if (todo.id.isNull()) {
        todo.id = QUuid::createUuid();
    }
    m_todos.insert(todo.id, todo);
    save();
    return todo;
}

bool FileCalendarStorage::removeTodo(const QUuid &id)
{
    if (m_todos.remove(id) > 0) {
        save();
        return true;
    }
    return false;
}

void FileCalendarStorage::load()
{
    m_events.clear();
    m_todos.clear();

    QFile file(m_filePath);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    enum class Section {
        None,
        Event,
        Todo
    };

    Section currentSection = Section::None;
    CalendarEvent currentEvent;
    TodoItem currentTodo;

    auto finalizeEvent = [&]() {
        if (currentEvent.id.isNull()) {
            currentEvent.id = QUuid::createUuid();
        }
        if (!currentEvent.end.isValid() || currentEvent.end <= currentEvent.start) {
            currentEvent.end = currentEvent.start.addSecs(30 * 60);
        }
        m_events.insert(currentEvent.id, currentEvent);
        currentEvent = CalendarEvent{};
    };

    auto finalizeTodo = [&]() {
        if (currentTodo.id.isNull()) {
            currentTodo.id = QUuid::createUuid();
        }
        m_todos.insert(currentTodo.id, currentTodo);
        currentTodo = TodoItem{};
    };

    auto handleLine = [&](const QString &line) {
        if (line == QLatin1String("BEGIN:VEVENT")) {
            currentSection = Section::Event;
            currentEvent = CalendarEvent{};
            return;
        }
        if (line == QLatin1String("END:VEVENT")) {
            finalizeEvent();
            currentSection = Section::None;
            return;
        }
        if (line == QLatin1String("BEGIN:VTODO")) {
            currentSection = Section::Todo;
            currentTodo = TodoItem{};
            return;
        }
        if (line == QLatin1String("END:VTODO")) {
            finalizeTodo();
            currentSection = Section::None;
            return;
        }

        if (currentSection == Section::None) {
            return;
        }

        const int colonIndex = line.indexOf(':');
        if (colonIndex <= 0) {
            return;
        }

        const QString property = line.left(colonIndex);
        const QString rawValue = line.mid(colonIndex + 1);
        const QString name = property.section(';', 0, 0).toUpper();
        const QString parameters = property.contains(';') ? property.section(';', 1) : QString();
        const QString value = decodeText(rawValue);

        const bool dateOnlyParam = parameters.contains(QLatin1String("VALUE=DATE"), Qt::CaseInsensitive);

        if (currentSection == Section::Event) {
            if (name == QLatin1String("UID")) {
                currentEvent.id = parseUid(value);
            } else if (name == QLatin1String("SUMMARY")) {
                currentEvent.title = value;
            } else if (name == QLatin1String("DESCRIPTION")) {
                currentEvent.description = value;
            } else if (name == QLatin1String("LOCATION")) {
                currentEvent.location = value;
            } else if (name == QLatin1String("DTSTART")) {
                currentEvent.start = parseDateTime(rawValue);
                currentEvent.allDay = dateOnlyParam || rawValue.size() == 8;
                if (currentEvent.allDay && currentEvent.start.isValid()) {
                    currentEvent.start.setTime(QTime(0, 0));
                }
            } else if (name == QLatin1String("DTEND")) {
                currentEvent.end = parseDateTime(rawValue);
                if (currentEvent.allDay && currentEvent.end.isValid()) {
                    currentEvent.end.setTime(QTime(0, 0));
                }
            } else if (name == QLatin1String("CATEGORIES")) {
                currentEvent.categories = splitList(value);
            } else if (name == QLatin1String("RRULE")) {
                currentEvent.recurrenceRule = rawValue;
            } else if (name == QLatin1String("X-TASKMASTER-REMINDER")) {
                currentEvent.reminderMinutes = rawValue.toInt();
            } else if (name == QLatin1String("X-TASKMASTER-ALLDAY")) {
                currentEvent.allDay = rawValue.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0;
            }
            return;
        }

        if (currentSection == Section::Todo) {
            if (name == QLatin1String("UID")) {
                currentTodo.id = parseUid(value);
            } else if (name == QLatin1String("SUMMARY")) {
                currentTodo.title = value;
            } else if (name == QLatin1String("DESCRIPTION")) {
                currentTodo.description = value;
            } else if (name == QLatin1String("LOCATION")) {
                currentTodo.location = value;
            } else if (name == QLatin1String("DUE")) {
                currentTodo.dueDate = parseDateTime(rawValue);
                if (dateOnlyParam && currentTodo.dueDate.isValid()) {
                    currentTodo.dueDate.setTime(QTime(0, 0));
                }
            } else if (name == QLatin1String("PRIORITY")) {
                currentTodo.priority = rawValue.toInt();
            } else if (name == QLatin1String("STATUS")) {
                currentTodo.status = statusFromString(rawValue);
            } else if (name == QLatin1String("CATEGORIES")) {
                currentTodo.tags = splitList(value);
            } else if (name == QLatin1String("X-TASKMASTER-SCHEDULED")) {
                currentTodo.scheduled = rawValue.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0;
            } else if (name == QLatin1String("X-TASKMASTER-DURATION")) {
                currentTodo.durationMinutes = rawValue.toInt();
            }
        }
    };

    QString accumulator;
    bool hasAccumulator = false;
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (!line.isEmpty() && (line.startsWith(' ') || line.startsWith('\t'))) {
            if (hasAccumulator) {
                accumulator += line.mid(1);
            }
        } else {
            if (hasAccumulator) {
                handleLine(accumulator);
            }
            accumulator = line;
            hasAccumulator = true;
        }
    }
    if (hasAccumulator) {
        handleLine(accumulator);
    }
}

void FileCalendarStorage::save() const
{
    if (m_filePath.isEmpty()) {
        return;
    }

    QFileInfo info(m_filePath);
    QDir dir = info.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    stream << "BEGIN:VCALENDAR\n";
    stream << "VERSION:2.0\n";
    stream << "PRODID:-//Block Master//EN\n";

    auto events = m_events.values();
    std::sort(events.begin(), events.end(), [](const CalendarEvent &lhs, const CalendarEvent &rhs) {
        return lhs.start < rhs.start;
    });
    for (const CalendarEvent &event : events) {
        stream << "BEGIN:VEVENT\n";
        stream << "UID:" << prepareUid(event.id) << '\n';
        stream << "SUMMARY:" << encodeText(event.title) << '\n';
        if (!event.description.isEmpty()) {
            stream << "DESCRIPTION:" << encodeText(event.description) << '\n';
        }
        if (!event.location.isEmpty()) {
            stream << "LOCATION:" << encodeText(event.location) << '\n';
        }
        if (event.allDay) {
            stream << "DTSTART;VALUE=DATE:" << event.start.date().toString(DATE_FORMAT) << '\n';
            stream << "DTEND;VALUE=DATE:" << event.end.date().toString(DATE_FORMAT) << '\n';
        } else {
            stream << "DTSTART:" << formatDateTime(event.start) << '\n';
            stream << "DTEND:" << formatDateTime(event.end) << '\n';
        }
        if (!event.categories.isEmpty()) {
            stream << "CATEGORIES:" << encodeText(event.categories.join(',')) << '\n';
        }
        if (!event.recurrenceRule.isEmpty()) {
            stream << "RRULE:" << event.recurrenceRule << '\n';
        }
        if (event.reminderMinutes > 0) {
            stream << "X-TASKMASTER-REMINDER:" << event.reminderMinutes << '\n';
        }
        if (event.allDay) {
            stream << "X-TASKMASTER-ALLDAY:TRUE\n";
        }
        stream << "END:VEVENT\n";
    }

    auto todos = m_todos.values();
    std::sort(todos.begin(), todos.end(), [](const TodoItem &lhs, const TodoItem &rhs) {
        return lhs.priority > rhs.priority;
    });
    for (const TodoItem &todo : todos) {
        stream << "BEGIN:VTODO\n";
        stream << "UID:" << prepareUid(todo.id) << '\n';
        stream << "SUMMARY:" << encodeText(todo.title) << '\n';
        if (!todo.description.isEmpty()) {
            stream << "DESCRIPTION:" << encodeText(todo.description) << '\n';
        }
        if (!todo.location.isEmpty()) {
            stream << "LOCATION:" << encodeText(todo.location) << '\n';
        }
        if (todo.dueDate.isValid()) {
            stream << "DUE:" << formatDateTime(todo.dueDate) << '\n';
        }
        if (todo.priority > 0) {
            stream << "PRIORITY:" << todo.priority << '\n';
        }
        stream << "STATUS:" << statusToString(todo.status) << '\n';
        if (!todo.tags.isEmpty()) {
            stream << "CATEGORIES:" << encodeText(todo.tags.join(',')) << '\n';
        }
        if (todo.scheduled) {
            stream << "X-TASKMASTER-SCHEDULED:TRUE\n";
        }
        if (todo.durationMinutes > 0) {
            stream << "X-TASKMASTER-DURATION:" << todo.durationMinutes << '\n';
        }
        stream << "END:VTODO\n";
    }

    stream << "END:VCALENDAR\n";

    stream.flush();
    file.commit();
}

QString FileCalendarStorage::encodeText(const QString &text)
{
    QString encoded = text;
    encoded.replace('\\', "\\\\");
    encoded.replace('\n', "\\n");
    encoded.replace(',', "\\,");
    encoded.replace(';', "\\;");
    return encoded;
}

QString FileCalendarStorage::decodeText(const QString &text)
{
    QString decoded = text;
    decoded.replace("\\n", "\n", Qt::CaseInsensitive);
    decoded.replace("\\,", ",");
    decoded.replace("\\;", ";");
    decoded.replace("\\\\", "\\");
    return decoded;
}

QString FileCalendarStorage::formatDateTime(const QDateTime &dt)
{
    if (!dt.isValid()) {
        return {};
    }
    return dt.toUTC().toString(QLatin1String(DATE_TIME_FORMAT));
}

QDateTime FileCalendarStorage::parseDateTime(const QString &value)
{
    if (value.length() == 8) {
        const QDate date = QDate::fromString(value, DATE_FORMAT);
        return QDateTime(date.startOfDay());
    }
    if (value.endsWith('Z')) {
        QDateTime dt = QDateTime::fromString(value, DATE_TIME_FORMAT);
        dt.setTimeSpec(Qt::UTC);
        return dt.toLocalTime();
    }
    QDateTime dt = QDateTime::fromString(value, "yyyyMMdd'T'hhmmss");
    if (!dt.isValid()) {
        dt = QDateTime::fromString(value, Qt::ISODate);
    }
    return dt;
}

QString FileCalendarStorage::statusToString(TodoStatus status)
{
    switch (status) {
    case TodoStatus::Completed:
        return QStringLiteral("COMPLETED");
    case TodoStatus::InProgress:
        return QStringLiteral("IN-PROCESS");
    case TodoStatus::Pending:
    default:
        return QStringLiteral("NEEDS-ACTION");
    }
}

TodoStatus FileCalendarStorage::statusFromString(const QString &value)
{
    const QString normalized = value.toUpper();
    if (normalized == QLatin1String("COMPLETED")) {
        return TodoStatus::Completed;
    }
    if (normalized == QLatin1String("IN-PROCESS")) {
        return TodoStatus::InProgress;
    }
    return TodoStatus::Pending;
}

} // namespace data
} // namespace calendar
