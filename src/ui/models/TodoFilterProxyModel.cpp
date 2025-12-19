#include "calendar/ui/models/TodoFilterProxyModel.hpp"

#include "calendar/ui/models/TodoListModel.hpp"

namespace calendar {
namespace ui {

TodoFilterProxyModel::TodoFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void TodoFilterProxyModel::setFilterText(const QString &text)
{
    if (m_filterText == text) {
        return;
    }
    m_filterText = text;
    invalidateFilter();
}

void TodoFilterProxyModel::setStatusFilter(std::optional<data::TodoStatus> status)
{
    if (m_statusFilter == status) {
        return;
    }
    m_statusFilter = status;
    invalidateFilter();
}

bool TodoFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const auto index = sourceModel()->index(sourceRow, 0, sourceParent);
    auto *todoModel = qobject_cast<TodoListModel *>(sourceModel());
    if (!todoModel) {
        return true;
    }
    const auto *todo = todoModel->todoAt(index);
    if (!todo) {
        return true;
    }

    if (!m_filterText.isEmpty()) {
        const auto text = m_filterText.toLower();
        if (!todo->title.toLower().contains(text) && !todo->description.toLower().contains(text)) {
            return false;
        }
    }

    if (m_statusFilter.has_value() && todo->status != m_statusFilter.value()) {
        return false;
    }

    return true;
}

} // namespace ui
} // namespace calendar
