#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QHash>
#include <QColor>

#include "calendar/data/Todo.hpp"

namespace calendar {
namespace ui {

class TodoListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit TodoListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    QStringList mimeTypes() const override;

    void setTodos(QVector<data::TodoItem> todos);
    const data::TodoItem *todoAt(const QModelIndex &index) const;
    void setKeywordColors(QHash<QString, QColor> colors);

signals:
    void todoActivated(const data::TodoItem &todo);

private:
    QVector<data::TodoItem> m_todos;
    QHash<QString, QColor> m_keywordColors;

    QColor keywordColorFor(const data::TodoItem &todo) const;
};

} // namespace ui
} // namespace calendar
