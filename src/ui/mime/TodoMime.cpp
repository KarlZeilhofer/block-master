#include "calendar/ui/mime/TodoMime.hpp"

#include <QBuffer>
#include <QDataStream>

namespace calendar {
namespace ui {

namespace {
constexpr quint32 CurrentTodoMimeVersion = 1;

QString sanitizeTitle(const QString &value)
{
    return value;
}
} // namespace

QByteArray encodeTodoMime(const QList<TodoMimeEntry> &entries)
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream << TodoMimeMagic << CurrentTodoMimeVersion << quint32(entries.size());
    for (const auto &entry : entries) {
        stream << entry.id << sanitizeTitle(entry.title) << entry.durationMinutes;
    }
    return buffer;
}

QVector<TodoMimeEntry> decodeTodoMime(const QByteArray &payload)
{
    QVector<TodoMimeEntry> entries;
    if (payload.isEmpty()) {
        return entries;
    }
    QBuffer buffer(const_cast<QByteArray *>(&payload));
    buffer.open(QIODevice::ReadOnly);
    QDataStream stream(&buffer);

    quint32 magic = 0;
    stream >> magic;
    if (magic == TodoMimeMagic) {
        quint32 version = 0;
        quint32 count = 0;
        stream >> version >> count;
        for (quint32 i = 0; i < count && !stream.atEnd(); ++i) {
            TodoMimeEntry entry;
            stream >> entry.id >> entry.title;
            if (version >= 1) {
                stream >> entry.durationMinutes;
            }
            entries.append(entry);
        }
        return entries;
    }

    buffer.seek(0);
    while (!stream.atEnd()) {
        TodoMimeEntry entry;
        stream >> entry.id >> entry.title;
        if (!entry.id.isNull()) {
            entries.append(entry);
        }
    }
    return entries;
}

std::optional<TodoMimeEntry> firstTodoMimeEntry(const QByteArray &payload)
{
    const auto entries = decodeTodoMime(payload);
    if (entries.isEmpty()) {
        return std::nullopt;
    }
    return entries.front();
}

} // namespace ui
} // namespace calendar
