#include "calendar/ui/widgets/TodoListView.hpp"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>
#include <QIODevice>

namespace {
constexpr auto TodoMimeType = "application/x-calendar-todo";
}

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
    QByteArray payload = mime->data(TodoMimeType);
    QDataStream stream(&payload, QIODevice::ReadOnly);
    while (!stream.atEnd()) {
        QUuid id;
        QString title;
        stream >> id >> title;
        if (!id.isNull()) {
            ids << id;
        }
    }
    return ids;
}

} // namespace ui
} // namespace calendar
