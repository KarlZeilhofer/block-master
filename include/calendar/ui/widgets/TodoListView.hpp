#pragma once

#include <QListView>
#include <QList>
#include <QUuid>

#include "calendar/data/Todo.hpp"

namespace calendar {
namespace ui {

class TodoListView : public QListView
{
    Q_OBJECT

public:
    explicit TodoListView(QWidget *parent = nullptr);

    void setTargetStatus(data::TodoStatus status);
    data::TodoStatus targetStatus() const;

signals:
    void todosDropped(const QList<QUuid> &todoIds, data::TodoStatus status);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    bool acceptTodoMime(const QMimeData *mime) const;
    QList<QUuid> decodeTodoIds(const QMimeData *mime) const;

    data::TodoStatus m_targetStatus = data::TodoStatus::Pending;
};

} // namespace ui
} // namespace calendar
