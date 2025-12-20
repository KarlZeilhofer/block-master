#pragma once

#include <QByteArray>
#include <QList>
#include <QVector>
#include <QtGlobal>
#include <optional>

#include <QString>
#include <QUuid>

namespace calendar {
namespace ui {

constexpr const char *TodoMimeType = "application/x-calendar-todo";
constexpr quint32 TodoMimeMagic = 0x544F444F; // "TODO"

struct TodoMimeEntry
{
    QUuid id;
    QString title;
    int durationMinutes = 0;
};

QByteArray encodeTodoMime(const QList<TodoMimeEntry> &entries);
QVector<TodoMimeEntry> decodeTodoMime(const QByteArray &payload);
std::optional<TodoMimeEntry> firstTodoMimeEntry(const QByteArray &payload);

} // namespace ui
} // namespace calendar
