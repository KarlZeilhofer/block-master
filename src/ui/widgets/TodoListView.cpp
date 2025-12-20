#include "calendar/ui/widgets/TodoListView.hpp"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

#include "calendar/ui/mime/TodoMime.hpp"

namespace calendar {
namespace ui {

TodoListView::TodoListView(QWidget *parent)
    : QListView(parent)
{
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
}

void TodoListView::setTargetStatus(data::TodoStatus status)
{
    m_targetStatus = status;
    setProperty("todoStatus", static_cast<int>(status));
}

data::TodoStatus TodoListView::targetStatus() const
{
    return m_targetStatus;
}

void TodoListView::dragEnterEvent(QDragEnterEvent *event)
{
    if (acceptTodoMime(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QListView::dragEnterEvent(event);
}

void TodoListView::dragMoveEvent(QDragMoveEvent *event)
{
    if (acceptTodoMime(event->mimeData())) {
        event->acceptProposedAction();
        return;
    }
    QListView::dragMoveEvent(event);
}

void TodoListView::dropEvent(QDropEvent *event)
{
    if (!acceptTodoMime(event->mimeData())) {
        QListView::dropEvent(event);
        return;
    }
    const QList<QUuid> ids = decodeTodoIds(event->mimeData());
    if (!ids.isEmpty()) {
        emit todosDropped(ids, m_targetStatus);
    }
    event->acceptProposedAction();
}

bool TodoListView::acceptTodoMime(const QMimeData *mime) const
{
    return mime && mime->hasFormat(TodoMimeType);
}

QList<QUuid> TodoListView::decodeTodoIds(const QMimeData *mime) const
{
    QList<QUuid> ids;
    if (!mime || !mime->hasFormat(TodoMimeType)) {
        return ids;
    }
    const auto entries = decodeTodoMime(mime->data(TodoMimeType));
    for (const auto &entry : entries) {
        if (!entry.id.isNull()) {
            ids << entry.id;
        }
    }
    return ids;
}

} // namespace ui
} // namespace calendar
