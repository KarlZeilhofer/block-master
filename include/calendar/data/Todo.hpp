#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QUuid>

namespace calendar {
namespace data {

enum class TodoStatus
{
    Pending,
    InProgress,
    Completed,
};

struct TodoItem
{
    QUuid id = QUuid::createUuid();
    QString title;
    QString description;
    QString location;
    QDateTime dueDate;
    int priority = 0;
    QStringList tags;
    TodoStatus status = TodoStatus::Pending;
    bool scheduled = false;
    int durationMinutes = 0;
};

} // namespace data
} // namespace calendar
