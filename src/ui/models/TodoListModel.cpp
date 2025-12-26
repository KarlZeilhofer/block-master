#include "calendar/ui/models/TodoListModel.hpp"

#include <QDataStream>
#include <QMimeData>
#include <QRegularExpression>

#include "calendar/ui/mime/TodoMime.hpp"

namespace calendar {
namespace ui {

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
    case Qt::DisplayRole: {
        QString display = todo.title;
        if (todo.durationMinutes > 0) {
            const int hours = todo.durationMinutes / 60;
            const int minutes = todo.durationMinutes % 60;
            QString durationText;
            if (hours > 0 && minutes > 0) {
                durationText = tr("%1h %2m").arg(hours).arg(minutes);
            } else if (hours > 0) {
                durationText = tr("%1h").arg(hours);
            } else if (minutes > 0) {
                durationText = tr("%1m").arg(minutes);
            }
            if (!durationText.isEmpty()) {
                display += tr(" (%1)").arg(durationText);
            }
        }
        return display;
    }
    case Qt::ToolTipRole:
        return todo.description;
    case Qt::ForegroundRole: {
        QColor keywordColor = keywordColorFor(todo);
        if (keywordColor.isValid()) {
            return keywordColor;
        }
        return {};
    }
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
    QList<TodoMimeEntry> entries;

    for (const auto &index : indexes) {
        if (!index.isValid()) {
            continue;
        }
        const auto &todo = m_todos.at(index.row());
        entries.append({ todo.id, todo.title, todo.durationMinutes });
    }
    mime->setData(TodoMimeType, encodeTodoMime(entries));
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

void TodoListModel::setKeywordColors(QHash<QString, QColor> colors)
{
    m_keywordColors = std::move(colors);
    if (m_todos.isEmpty()) {
        return;
    }
    const QModelIndex first = index(0, 0);
    const QModelIndex last = index(m_todos.size() - 1, 0);
    emit dataChanged(first, last, { Qt::ForegroundRole });
}

QColor TodoListModel::keywordColorFor(const data::TodoItem &todo) const
{
    if (m_keywordColors.isEmpty()) {
        return {};
    }
    static const QRegularExpression tagRegex(QStringLiteral("#([A-Za-z0-9_ÄÖÜäöüß]+)"));
    auto matchColor = [&](const QString &text) -> QColor {
        if (text.isEmpty()) {
            return {};
        }
        auto it = tagRegex.globalMatch(text);
        while (it.hasNext()) {
            const QString tag = it.next().captured(1).toLower();
            auto colorIt = m_keywordColors.constFind(tag);
            if (colorIt != m_keywordColors.constEnd()) {
                return colorIt.value();
            }
        }
        return {};
    };
    QColor color = matchColor(todo.title);
    if (!color.isValid()) {
        color = matchColor(todo.description);
    }
    if (!color.isValid()) {
        color = matchColor(todo.location);
    }
    return color;
}

} // namespace ui
} // namespace calendar
