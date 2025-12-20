#include "calendar/ui/widgets/TodoListView.hpp"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>

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
        showGhostPreview(labelForMime(event->mimeData()));
        event->acceptProposedAction();
        return;
    }
    clearGhostPreview();
    QListView::dragEnterEvent(event);
}

void TodoListView::dragMoveEvent(QDragMoveEvent *event)
{
    if (acceptTodoMime(event->mimeData())) {
        showGhostPreview(labelForMime(event->mimeData()));
        event->acceptProposedAction();
        return;
    }
    clearGhostPreview();
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
    clearGhostPreview();
}

void TodoListView::dragLeaveEvent(QDragLeaveEvent *event)
{
    clearGhostPreview();
    QListView::dragLeaveEvent(event);
}

void TodoListView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    if (!m_showGhostPreview || !viewport()) {
        return;
    }
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    QRectF rect = viewport()->rect().adjusted(6, 6, -6, -6);
    QColor fill = palette().highlight().color();
    fill.setAlpha(90);
    painter.setBrush(fill);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect, 8, 8);

    painter.setPen(palette().brightText().color());
    painter.drawText(rect.adjusted(8, 4, -8, -4),
                     Qt::AlignCenter | Qt::TextWordWrap,
                     m_ghostText);
}

void TodoListView::showGhostPreview(const QString &text)
{
    if (m_showGhostPreview && m_ghostText == text) {
        return;
    }
    m_showGhostPreview = true;
    m_ghostText = text;
    if (viewport()) {
        viewport()->update();
    } else {
        update();
    }
}

void TodoListView::clearGhostPreview()
{
    if (!m_showGhostPreview) {
        return;
    }
    m_showGhostPreview = false;
    m_ghostText.clear();
    if (viewport()) {
        viewport()->update();
    } else {
        update();
    }
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

QString TodoListView::labelForMime(const QMimeData *mime) const
{
    if (!mime || !mime->hasFormat(TodoMimeType)) {
        return tr("TODOs verschieben");
    }
    const auto entries = decodeTodoMime(mime->data(TodoMimeType));
    if (entries.isEmpty()) {
        return tr("TODOs verschieben");
    }
    if (entries.size() > 1) {
        return tr("%1 TODOs").arg(entries.size());
    }
    const auto &entry = entries.first();
    QString label = entry.title.isEmpty() ? tr("(Ohne Titel)") : entry.title;
    if (entry.durationMinutes > 0) {
        const int hours = entry.durationMinutes / 60;
        const int minutes = entry.durationMinutes % 60;
        QString durationText;
        if (hours > 0 && minutes > 0) {
            durationText = tr("%1h %2m").arg(hours).arg(minutes);
        } else if (hours > 0) {
            durationText = tr("%1h").arg(hours);
        } else if (minutes > 0) {
            durationText = tr("%1m").arg(minutes);
        }
        if (!durationText.isEmpty()) {
            label += tr(" (%1)").arg(durationText);
        }
    }
    return label;
}

} // namespace ui
} // namespace calendar
