#pragma once

#include <QSortFilterProxyModel>
#include <optional>

#include "calendar/data/Todo.hpp"

namespace calendar {
namespace ui {

class TodoFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit TodoFilterProxyModel(QObject *parent = nullptr);

    void setFilterText(const QString &text);
    void setStatusFilter(std::optional<data::TodoStatus> status);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterText;
    std::optional<data::TodoStatus> m_statusFilter;
};

} // namespace ui
} // namespace calendar
