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
    QDateTime dueDate;
    int priority = 0;
    QStringList tags;
    TodoStatus status = TodoStatus::Pending;
};

} // namespace data
} // namespace calendar
