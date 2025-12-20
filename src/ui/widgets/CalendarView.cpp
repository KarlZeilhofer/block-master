#include "calendar/ui/widgets/CalendarView.hpp"

#include <QApplication>
#include <QDataStream>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTime>
#include <QLocale>
#include <QtMath>
#include <QIODevice>
#include <algorithm>

namespace calendar {
namespace ui {

namespace {
constexpr double MinHourHeight = 20.0;
constexpr double MaxHourHeight = 160.0;
constexpr double MinDayWidth = 120.0;
constexpr double MaxDayWidth = 420.0;
constexpr double HandleZone = 8.0;
constexpr int SnapIntervalMinutes = 15;
const char *TodoMimeType = "application/x-calendar-todo";
const char *EventMimeType = "application/x-calendar-event";
constexpr double HandleHoverRange = 12.0;
} // namespace

CalendarView::CalendarView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setDateRange(QDate::currentDate(), m_dayCount);
    updateScrollBars();
}

void CalendarView::setDateRange(const QDate &start, int days)
{
    if (!start.isValid() || days <= 0) {
        return;
    }
    m_startDate = start;
    m_dayCount = days;
    viewport()->update();
    updateScrollBars();
}

void CalendarView::setEvents(std::vector<data::CalendarEvent> events)
{
    m_events = std::move(events);
    if (!m_selectedEvent.isNull()) {
        auto it = std::find_if(m_events.begin(), m_events.end(), [this](const data::CalendarEvent &ev) {
            return ev.id == m_selectedEvent;
        });
        if (it == m_events.end()) {
            m_selectedEvent = {};
        }
    }
    viewport()->update();
}

void CalendarView::zoomTime(double factor)
{
    m_hourHeight = qBound(MinHourHeight, m_hourHeight * factor, MaxHourHeight);
    updateScrollBars();
    viewport()->update();
}

void CalendarView::zoomDays(double factor)
{
    m_dayWidth = qBound(MinDayWidth, m_dayWidth * factor, MaxDayWidth);
    updateScrollBars();
    viewport()->update();
}

void CalendarView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), palette().base());

    const double totalHeight = m_hourHeight * 24.0;
    const QRectF vpRect = viewport()->rect();
    const double xOffset = horizontalScrollBar()->value();
    const double yOffset = verticalScrollBar()->value();

    painter.translate(-xOffset, -yOffset);

    const QRectF drawingRect(0, 0, m_timeAxisWidth + m_dayWidth * m_dayCount, m_headerHeight + totalHeight);

    painter.setPen(palette().mid().color());
    // Horizontal hour lines.
    for (int hour = 0; hour <= 24; ++hour) {
        const double y = m_headerHeight + hour * m_hourHeight;
        if (!drawingRect.intersects(QRectF(0, y - m_hourHeight, drawingRect.width(), m_hourHeight * 2))) {
            continue;
        }
        painter.drawLine(QPointF(0, y), QPointF(drawingRect.width(), y));
        painter.drawText(QRectF(0, y - m_hourHeight, m_timeAxisWidth - 4, m_hourHeight),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("%1:00").arg(hour, 2, 10, QLatin1Char('0')));
    }

    // Day columns
    for (int day = 0; day < m_dayCount; ++day) {
        const double x = m_timeAxisWidth + day * m_dayWidth;
        const QRectF columnRect(x, 0, m_dayWidth, m_headerHeight + totalHeight);
        if (!vpRect.translated(xOffset, yOffset).intersects(columnRect)) {
            continue;
        }
        painter.fillRect(QRectF(x, 0, m_dayWidth, m_headerHeight), palette().alternateBase());
        painter.setPen(palette().dark().color());
        painter.drawRect(QRectF(x, 0, m_dayWidth, m_headerHeight + totalHeight));

        const QDate date = m_startDate.addDays(day);
        painter.drawText(QRectF(x, 0, m_dayWidth, m_headerHeight),
                         Qt::AlignCenter,
                         QLocale().toString(date, QLocale::LongFormat));
    }

    // Events
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto &eventData : m_events) {
        const QRectF rect = eventRect(eventData);
        if (!rect.translated(0, 0).intersects(vpRect.translated(xOffset, yOffset))) {
            continue;
        }

        QColor color = palette().highlight().color();
        if (eventData.id == m_selectedEvent) {
            color = color.darker(125);
        }
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect, 4, 4);

        const double handleHeight = 6.0;
        QColor handleColor = palette().highlight().color().lighter(130);
        handleColor.setAlpha(160);
        painter.setPen(Qt::NoPen);
        if (eventData.id == m_hoverTopHandleId) {
            QRectF topHandle(rect.x() + 6, rect.y() - handleHeight / 2, rect.width() - 12, handleHeight);
            painter.setBrush(handleColor);
            painter.drawRoundedRect(topHandle, 3, 3);
            painter.setPen(QPen(palette().base().color(), 1));
            painter.drawLine(QPointF(topHandle.left() + 4, topHandle.center().y()),
                             QPointF(topHandle.right() - 4, topHandle.center().y()));
            painter.setPen(Qt::NoPen);
        }
        if (eventData.id == m_hoverBottomHandleId) {
            QRectF bottomHandle(rect.x() + 6, rect.bottom() - handleHeight / 2, rect.width() - 12, handleHeight);
            painter.setBrush(handleColor);
            painter.drawRoundedRect(bottomHandle, 3, 3);
            painter.setPen(QPen(palette().base().color(), 1));
            painter.drawLine(QPointF(bottomHandle.left() + 4, bottomHandle.center().y()),
                             QPointF(bottomHandle.right() - 4, bottomHandle.center().y()));
            painter.setPen(Qt::NoPen);
        }

        painter.setPen(Qt::white);
        painter.drawText(rect.adjusted(4, 2, -4, -2),
                         Qt::TextWordWrap,
                         QStringLiteral("%1\n%2 - %3")
                             .arg(eventData.title,
                                  eventData.start.time().toString(QStringLiteral("hh:mm")),
                                  eventData.end.time().toString(QStringLiteral("hh:mm"))));
    }

    if (m_showDropPreview && m_dropPreviewRect.isValid()) {
        painter.setBrush(QColor(100, 149, 237, 120));
        painter.setPen(QPen(palette().highlight().color(), 1, Qt::DashLine));
        painter.drawRoundedRect(m_dropPreviewRect, 4, 4);
        painter.setPen(Qt::white);
        painter.drawText(m_dropPreviewRect.adjusted(4, 2, -4, -2),
                         Qt::AlignLeft | Qt::AlignTop,
                         m_dropPreviewText);
    }
}

void CalendarView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

void CalendarView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        zoomDays(event->angleDelta().y() > 0 ? 1.1 : 0.9);
        event->accept();
        return;
    }
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        zoomTime(event->angleDelta().y() > 0 ? 1.1 : 0.9);
        event->accept();
        return;
    }
    const QPoint angle = event->angleDelta();
    bool handled = false;
    if (angle.y() != 0) {
        const double steps = angle.y() / 120.0;
        const double delta = steps * (m_hourHeight * 0.25); // 15 Minuten
        auto *vbar = verticalScrollBar();
        const int newValue = qBound(vbar->minimum(), vbar->value() - static_cast<int>(delta), vbar->maximum());
        vbar->setValue(newValue);
        handled = true;
    }
    if (angle.x() != 0) {
        const double steps = angle.x() / 120.0;
        const double delta = steps * (m_dayWidth * 0.25); // Viertel Tag
        auto *hbar = horizontalScrollBar();
        const int newValue = qBound(hbar->minimum(), hbar->value() - static_cast<int>(delta), hbar->maximum());
        hbar->setValue(newValue);
        handled = true;
    }
    if (handled) {
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void CalendarView::mousePressEvent(QMouseEvent *event)
{
    const QPointF scenePos = QPointF(event->pos())
        + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    if (m_externalPlacementMode && event->button() == Qt::LeftButton) {
        auto dateTimeOpt = dateTimeAtScene(scenePos);
        if (dateTimeOpt.has_value()) {
            QDateTime dateTime = dateTimeOpt.value();
            int minutes = snapMinutes(dateTime.time().hour() * 60 + dateTime.time().minute());
            dateTime.setTime(QTime(minutes / 60, minutes % 60));
            cancelPlacementPreview();
            emit externalPlacementConfirmed(dateTime);
        } else {
            cancelPlacementPreview();
        }
        event->accept();
        return;
    }
    if (m_externalPlacementMode && event->button() == Qt::RightButton) {
        cancelPlacementPreview();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->pos();
        resetDragCandidate();
        for (const auto &ev : m_events) {
            const QRectF rect = eventRect(ev);
            const bool withinX = scenePos.x() >= rect.left() && scenePos.x() <= rect.right();
            if (withinX) {
                auto topArea = handleArea(ev, true);
                if (scenePos.y() >= topArea.first && scenePos.y() <= topArea.second) {
                    beginResize(ev, true);
                    event->accept();
                    return;
                }
                auto bottomArea = handleArea(ev, false);
                if (scenePos.y() >= bottomArea.first && scenePos.y() <= bottomArea.second) {
                    beginResize(ev, false);
                    event->accept();
                    return;
                }
            }
            if (!rect.contains(scenePos)) {
                continue;
            }
            m_dragCandidateId = ev.id;
            const double pointerMinutes = ((scenePos.y() - m_headerHeight) / m_hourHeight) * 60.0;
            const double eventStartMinutes = ev.start.time().hour() * 60 + ev.start.time().minute();
            int rawOffset = static_cast<int>(pointerMinutes - eventStartMinutes);
            rawOffset = snapIntervalMinutes(rawOffset);
            const int durationMinutes = ev.start.secsTo(ev.end) / 60;
            m_dragPointerOffsetMinutes = qBound(0, rawOffset, durationMinutes);

            m_selectedEvent = ev.id;
            viewport()->update();
            emit eventActivated(ev);
            emit eventSelected(ev);
            event->accept();
            return;
        }
        m_selectedEvent = {};
        viewport()->update();
        emit selectionCleared();
        event->accept();
        return;
    }
    QAbstractScrollArea::mousePressEvent(event);
}

void CalendarView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragMode != DragMode::None) {
        const QPointF scenePos = QPointF(event->pos())
            + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
        updateResize(scenePos);
        event->accept();
        return;
    }
    if ((event->buttons() & Qt::LeftButton) && !m_dragCandidateId.isNull()) {
        if ((event->pos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            auto it = std::find_if(m_events.begin(), m_events.end(), [this](const data::CalendarEvent &ev) {
                return ev.id == m_dragCandidateId;
            });
            if (it != m_events.end()) {
                startEventDrag(*it, m_dragPointerOffsetMinutes);
                event->accept();
                return;
            }
            resetDragCandidate();
        }
    }
    emitHoverAt(event->pos());
    QAbstractScrollArea::mouseMoveEvent(event);
}

void CalendarView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragMode != DragMode::None && event->button() == Qt::LeftButton) {
        endResize();
        event->accept();
        return;
    }
    resetDragCandidate();
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void CalendarView::dragEnterEvent(QDragEnterEvent *event)
{
    cancelPlacementPreview();
    const auto *mime = event->mimeData();
    if (mime->hasFormat(TodoMimeType) || mime->hasFormat(EventMimeType)) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void CalendarView::dragMoveEvent(QDragMoveEvent *event)
{
    const auto *mime = event->mimeData();
    const QPointF scenePos = QPointF(event->pos())
        + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());

    if (mime->hasFormat(TodoMimeType)) {
        QByteArray payload = mime->data(TodoMimeType);
        QDataStream stream(&payload, QIODevice::ReadOnly);
        QUuid todoId;
        QString title;
        stream >> todoId >> title;
        auto dateTimeOpt = dateTimeAtScene(scenePos);
        if (!dateTimeOpt) {
            clearDropPreview();
            event->ignore();
            return;
        }
        QDateTime dateTime = dateTimeOpt.value();
        int minutes = snapMinutes(dateTime.time().hour() * 60 + dateTime.time().minute());
        dateTime.setTime(QTime(minutes / 60, minutes % 60));
        const QString label = title.isEmpty() ? tr("Neuer Termin") : title;
        updateDropPreview(dateTime, 60, label);
        event->acceptProposedAction();
        return;
    }

    if (mime->hasFormat(EventMimeType)) {
        QByteArray payload = mime->data(EventMimeType);
        QDataStream stream(&payload, QIODevice::ReadOnly);
        QUuid eventId;
        int offsetMinutes = 0;
        stream >> eventId >> offsetMinutes;
        auto dateTimeOpt = dateTimeAtScene(scenePos);
        if (!dateTimeOpt) {
            clearDropPreview();
            event->ignore();
            return;
        }
        QDateTime dateTime = dateTimeOpt.value();
        int minutes = snapMinutes(dateTime.time().hour() * 60 + dateTime.time().minute());
        dateTime.setTime(QTime(minutes / 60, minutes % 60));
        dateTime = dateTime.addSecs(-offsetMinutes * 60);
        minutes = snapMinutes(dateTime.time().hour() * 60 + dateTime.time().minute());
        dateTime.setTime(QTime(minutes / 60, minutes % 60));
        auto it = std::find_if(m_events.begin(), m_events.end(), [eventId](const data::CalendarEvent &ev) {
            return ev.id == eventId;
        });
        if (it == m_events.end()) {
            clearDropPreview();
            event->ignore();
            return;
        }
        const int durationMinutes = qMax<int>(30, it->start.secsTo(it->end) / 60);
        updateDropPreview(dateTime, durationMinutes, it->title);
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void CalendarView::dropEvent(QDropEvent *event)
{
    const auto *mime = event->mimeData();
    const QPointF scenePos = QPointF(event->pos())
        + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    clearDropPreview();

    if (mime->hasFormat(TodoMimeType)) {
        QByteArray payload = mime->data(TodoMimeType);
        QDataStream stream(&payload, QIODevice::ReadOnly);
        QUuid todoId;
        QString title;
        stream >> todoId >> title;
        auto dateTimeOpt = dateTimeAtScene(scenePos);
        if (!dateTimeOpt.has_value()) {
            event->ignore();
            return;
        }
        QDateTime dateTime = dateTimeOpt.value();
        int minutes = dateTime.time().hour() * 60 + dateTime.time().minute();
        minutes = snapMinutes(minutes);
        dateTime.setTime(QTime(minutes / 60, minutes % 60));
        emit todoDropped(todoId, dateTime);
        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }

    if (mime->hasFormat(EventMimeType)) {
        QByteArray payload = mime->data(EventMimeType);
        QDataStream stream(&payload, QIODevice::ReadOnly);
        QUuid eventId;
        int offsetMinutes = 0;
        stream >> eventId >> offsetMinutes;
        offsetMinutes = snapIntervalMinutes(offsetMinutes);
        auto dateTimeOpt = dateTimeAtScene(scenePos);
        if (!dateTimeOpt.has_value()) {
            event->ignore();
            return;
        }
        QDateTime dateTime = dateTimeOpt.value();
        int minutes = dateTime.time().hour() * 60 + dateTime.time().minute();
        minutes = snapMinutes(minutes);
        dateTime.setTime(QTime(minutes / 60, minutes % 60));
        dateTime = dateTime.addSecs(-offsetMinutes * 60);
        minutes = snapMinutes(dateTime.time().hour() * 60 + dateTime.time().minute());
        dateTime.setTime(QTime(minutes / 60, minutes % 60));
        bool copy = event->keyboardModifiers().testFlag(Qt::ControlModifier);
        emit eventDropRequested(eventId, dateTime, copy);
        event->setDropAction(copy ? Qt::CopyAction : Qt::MoveAction);
        event->accept();
        return;
    }

    event->ignore();
}

void CalendarView::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    clearDropPreview();
}

void CalendarView::beginPlacementPreview(int durationMinutes, const QString &label, const QDateTime &initialStart)
{
    m_externalPlacementMode = true;
    m_externalPlacementDuration = durationMinutes;
    m_externalPlacementLabel = label;
    updatePlacementPreview(initialStart);
}

void CalendarView::updatePlacementPreview(const QDateTime &start)
{
    if (!m_externalPlacementMode) {
        return;
    }
    updateDropPreview(start, m_externalPlacementDuration, m_externalPlacementLabel);
}

void CalendarView::cancelPlacementPreview()
{
    if (!m_externalPlacementMode) {
        return;
    }
    m_externalPlacementMode = false;
    m_externalPlacementDuration = 0;
    m_externalPlacementLabel.clear();
    clearDropPreview();
}

QRectF CalendarView::eventRect(const data::CalendarEvent &event) const
{
    const int dayIndex = m_startDate.daysTo(event.start.date());
    if (dayIndex < 0 || dayIndex >= m_dayCount) {
        return {};
    }

    const double x = m_timeAxisWidth + dayIndex * m_dayWidth + 6;
    const double startMinutes = event.start.time().hour() * 60 + event.start.time().minute();
    const double endMinutes = event.end.time().hour() * 60 + event.end.time().minute();
    const double y = m_headerHeight + (startMinutes / 60.0) * m_hourHeight;
    const double height = qMax((endMinutes - startMinutes) / 60.0 * m_hourHeight, 20.0);
    const double width = m_dayWidth - 12;

    return QRectF(x, y, width, height);
}

void CalendarView::updateScrollBars()
{
    const double totalWidth = m_timeAxisWidth + m_dayWidth * m_dayCount;
    const double totalHeight = m_headerHeight + m_hourHeight * 24.0;
    horizontalScrollBar()->setRange(0, qMax(0, static_cast<int>(totalWidth - viewport()->width())));
    horizontalScrollBar()->setPageStep(viewport()->width());

    verticalScrollBar()->setRange(0, qMax(0, static_cast<int>(totalHeight - viewport()->height())));
    verticalScrollBar()->setPageStep(viewport()->height());
}

void CalendarView::selectEventAt(const QPoint &pos)
{
    const QPointF scenePos = QPointF(pos) + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    for (const auto &eventData : m_events) {
        if (eventRect(eventData).contains(scenePos)) {
            m_selectedEvent = eventData.id;
            viewport()->update();
            emit eventActivated(eventData);
            emit eventSelected(eventData);
            return;
        }
    }
    m_selectedEvent = {};
    viewport()->update();
    emit selectionCleared();
}

void CalendarView::emitHoverAt(const QPoint &pos)
{
    const QPointF scenePos = QPointF(pos) + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const double y = scenePos.y() - m_headerHeight;
    if (y >= 0) {
        const double hours = y / m_hourHeight;
        const int hour = static_cast<int>(hours);
        const int minute = static_cast<int>((hours - hour) * 60);
        const double x = scenePos.x() - m_timeAxisWidth;
        const int dayIndex = static_cast<int>(x / m_dayWidth);
        if (dayIndex >= 0 && dayIndex < m_dayCount) {
            QDateTime dt(m_startDate.addDays(dayIndex), QTime(hour, minute));
            emit hoveredDateTime(dt);
        }
    }

    QUuid newTop;
    QUuid newBottom;
    for (const auto &eventData : m_events) {
        const QRectF rect = eventRect(eventData);
        if (scenePos.x() < rect.left() || scenePos.x() > rect.right()) {
            continue;
        }

        auto topArea = handleArea(eventData, true);
        if (scenePos.y() >= topArea.first && scenePos.y() <= topArea.second) {
            newTop = eventData.id;
        }

        auto bottomArea = handleArea(eventData, false);
        if (scenePos.y() >= bottomArea.first && scenePos.y() <= bottomArea.second) {
            newBottom = eventData.id;
        }

        if (!newTop.isNull() && !newBottom.isNull()) {
            break;
        }
    }

    if (newTop != m_hoverTopHandleId || newBottom != m_hoverBottomHandleId) {
        m_hoverTopHandleId = newTop;
        m_hoverBottomHandleId = newBottom;
        viewport()->update();
    }
}

int CalendarView::snapMinutes(double value) const
{
    const int snapped = static_cast<int>(qRound(value / SnapIntervalMinutes) * SnapIntervalMinutes);
    return qBound(0, snapped, 24 * 60);
}

int CalendarView::snapIntervalMinutes(int value) const
{
    if (SnapIntervalMinutes <= 0) {
        return value;
    }
    return static_cast<int>(qRound(static_cast<double>(value) / SnapIntervalMinutes) * SnapIntervalMinutes);
}

void CalendarView::beginResize(const data::CalendarEvent &event, bool adjustStart)
{
    m_dragEvent = event;
    m_dragMode = adjustStart ? DragMode::ResizeStart : DragMode::ResizeEnd;
    m_selectedEvent = event.id;
}

void CalendarView::updateResize(const QPointF &scenePos)
{
    if (m_dragMode == DragMode::None) {
        return;
    }
    const double y = scenePos.y() - m_headerHeight;
    if (y < 0) {
        return;
    }
    const double minutes = (y / m_hourHeight) * 60.0;
    const int snappedMinutes = snapMinutes(minutes);
    const QTime time = QTime(0, 0).addSecs(snappedMinutes * 60);

    if (m_dragMode == DragMode::ResizeStart) {
        const auto minEnd = m_dragEvent.end.addSecs(-SnapIntervalMinutes * 60);
        QDateTime newStart(m_dragEvent.start.date(), time);
        if (newStart < minEnd) {
            m_dragEvent.start = newStart;
        }
    } else if (m_dragMode == DragMode::ResizeEnd) {
        const auto minStart = m_dragEvent.start.addSecs(SnapIntervalMinutes * 60);
        QDateTime newEnd(m_dragEvent.end.date(), time);
        if (newEnd > minStart) {
            m_dragEvent.end = newEnd;
        }
    }

    for (auto &eventData : m_events) {
        if (eventData.id == m_dragEvent.id) {
            eventData = m_dragEvent;
            break;
        }
    }
    viewport()->update();
}

void CalendarView::endResize()
{
    if (m_dragMode == DragMode::None) {
        return;
    }
    emit eventResizeRequested(m_dragEvent.id, m_dragEvent.start, m_dragEvent.end);
    m_dragMode = DragMode::None;
}

std::optional<QDateTime> CalendarView::dateTimeAtScene(const QPointF &scenePos) const
{
    const double y = scenePos.y() - m_headerHeight;
    if (y < 0) {
        return std::nullopt;
    }
    const double x = scenePos.x() - m_timeAxisWidth;
    const int dayIndex = static_cast<int>(x / m_dayWidth);
    if (dayIndex < 0 || dayIndex >= m_dayCount) {
        return std::nullopt;
    }
    const double hours = y / m_hourHeight;
    int hour = qBound(0, static_cast<int>(hours), 23);
    int minute = qBound(0, static_cast<int>((hours - hour) * 60), 59);
    QDate date = m_startDate.addDays(dayIndex);
    return QDateTime(date, QTime(hour, minute));
}

const data::CalendarEvent *CalendarView::eventAt(const QPointF &scenePos) const
{
    for (const auto &eventData : m_events) {
        if (eventRect(eventData).contains(scenePos)) {
            return &eventData;
        }
    }
    return nullptr;
}

void CalendarView::startEventDrag(const data::CalendarEvent &event, int pointerOffsetMinutes)
{
    auto *drag = new QDrag(this);
    auto *mime = new QMimeData();
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    const int offset = snapIntervalMinutes(pointerOffsetMinutes);
    stream << event.id << offset;
    mime->setData(EventMimeType, payload);
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction | Qt::MoveAction);
    resetDragCandidate();
}

void CalendarView::resetDragCandidate()
{
    m_dragCandidateId = QUuid();
    m_dragPointerOffsetMinutes = 0;
}

void CalendarView::updateDropPreview(const QDateTime &start, int durationMinutes, const QString &label)
{
    if (!start.isValid() || durationMinutes <= 0) {
        clearDropPreview();
        return;
    }

    data::CalendarEvent preview;
    preview.start = start;
    preview.end = start.addSecs(durationMinutes * 60);

    const QRectF rect = eventRect(preview);
    if (rect.isEmpty()) {
        clearDropPreview();
        return;
    }

    m_dropPreviewRect = rect;
    m_dropPreviewText = label;
    if (!m_showDropPreview) {
        m_showDropPreview = true;
    }
    viewport()->update();
}

void CalendarView::clearDropPreview()
{
    if (!m_showDropPreview) {
        return;
    }
    m_showDropPreview = false;
    m_dropPreviewRect = QRectF();
    m_dropPreviewText.clear();
    viewport()->update();
}

QPair<double, double> CalendarView::handleArea(const data::CalendarEvent &event, bool top) const
{
    QRectF rect = eventRect(event);
    const double center = top ? rect.top() : rect.bottom();
    double minY = center - HandleHoverRange;
    double maxY = center + HandleHoverRange;

    const auto overlapsHorizontally = [&](const QRectF &other) {
        return !(other.right() <= rect.left() || other.left() >= rect.right());
    };

    for (const auto &other : m_events) {
        if (other.id == event.id) {
            continue;
        }
        QRectF otherRect = eventRect(other);
        if (!overlapsHorizontally(otherRect)) {
            continue;
        }
        if (top && otherRect.bottom() <= rect.top()) {
            minY = std::max(minY, otherRect.bottom());
        } else if (!top && otherRect.top() >= rect.bottom()) {
            maxY = std::min(maxY, otherRect.top());
        }
    }

    if (top) {
        maxY = std::min(maxY, rect.top() + HandleHoverRange);
    } else {
        minY = std::max(minY, rect.bottom() - HandleHoverRange);
    }

    if (minY > maxY) {
        minY = maxY = center;
    }

    return { minY, maxY };
}

} // namespace ui
} // namespace calendar
