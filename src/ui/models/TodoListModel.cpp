#include "calendar/ui/models/TodoListModel.hpp"

#include <QDataStream>
#include <QMimeData>

namespace calendar {
namespace ui {

static const char *TodoMimeType = "application/x-calendar-todo";

TodoListModel::TodoListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TodoListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_todos.size();
}

QVariant TodoListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_todos.size()) {
        return {};
    }

    const auto &todo = m_todos.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return todo.title;
    case Qt::ToolTipRole:
        return todo.description;
    default:
        return {};
    }
}

Qt::ItemFlags TodoListModel::flags(const QModelIndex &index) const
{
    auto defaultFlags = QAbstractListModel::flags(index);
    if (!index.isValid()) {
        return defaultFlags;
    }
    return defaultFlags | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

QMimeData *TodoListModel::mimeData(const QModelIndexList &indexes) const
{
    auto *mime = new QMimeData();
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);

    for (const auto &index : indexes) {
        if (!index.isValid()) {
            continue;
        }
        const auto &todo = m_todos.at(index.row());
        stream << todo.id << todo.title;
    }
    mime->setData(TodoMimeType, encoded);
    return mime;
}

QStringList TodoListModel::mimeTypes() const
{
    return { QString::fromLatin1(TodoMimeType) };
}

void TodoListModel::setTodos(QVector<data::TodoItem> todos)
{
    beginResetModel();
    m_todos = std::move(todos);
    endResetModel();
}

const data::TodoItem *TodoListModel::todoAt(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_todos.size()) {
        return nullptr;
    }
    return &m_todos.at(index.row());
}

} // namespace ui
} // namespace calendar
