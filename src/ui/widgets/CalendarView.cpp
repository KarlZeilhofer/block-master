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
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTime>
#include <QLocale>
#include <QtMath>
#include <QIODevice>
#include <QWheelEvent>
#include <QCoreApplication>
#include <QCursor>
#include <algorithm>
#include <map>

namespace calendar {
namespace ui {

namespace {
constexpr double MinHourHeight = 20.0;
constexpr double MaxHourHeight = 160.0;
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
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (qApp) {
        qApp->installEventFilter(this);
    }
    setDateRange(QDate::currentDate(), m_dayCount);
    updateScrollBars();
}

CalendarView::~CalendarView()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void CalendarView::setDateRange(const QDate &start, int days)
{
    if (!start.isValid() || days <= 0) {
        return;
    }
    m_startDate = start;
    m_dayCount = days;
    recalculateDayWidth();
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

void CalendarView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), palette().base());

    const double yOffset = verticalScrollBar()->value();
    const double bodyHeight = m_hourHeight * 24.0;
    const double bodyOriginY = m_headerHeight - yOffset;
    const double totalWidth = m_timeAxisWidth + m_dayWidth * m_dayCount;

    // Header background (sticky)
    painter.fillRect(QRectF(0, 0, viewport()->width(), m_headerHeight), palette().alternateBase());
    painter.setPen(palette().dark().color());
    painter.drawLine(QPointF(0, m_headerHeight - 0.5), QPointF(viewport()->width(), m_headerHeight - 0.5));

    std::map<double, QColor> highlightLines;
    const QDate today = QDate::currentDate();

    for (int day = 0; day < m_dayCount; ++day) {
        const double x = m_timeAxisWidth + day * m_dayWidth;
        QRectF headerRect(x, 0, m_dayWidth, m_headerHeight);
        const QDate date = m_startDate.addDays(day);
        const bool isSunday = date.dayOfWeek() == 7;
        const bool isToday = date == today;
        QColor headerColor = palette().alternateBase().color();
        QColor textColor = palette().windowText().color();
        if (isSunday) {
            headerColor = QColor(255, 235, 235);
            textColor = QColor(200, 40, 40);
        }
        painter.fillRect(headerRect, headerColor);
        painter.setPen(palette().dark().color());
        painter.drawRect(headerRect);
        painter.setPen(textColor);

        const QString dayText = QLocale().toString(date, QStringLiteral("dd"));
        const QString weekdayText = QLocale().toString(date, QStringLiteral("dddd"));
        const QString monthYearText = QLocale().toString(date, QStringLiteral("MMMM yyyy"));

        const double padding = 6.0;
        QRectF contentRect = headerRect.adjusted(padding, padding / 2, -padding, -padding / 2);
        const double dayBlockWidth = qMax(40.0, contentRect.width() * 0.35);
        QRectF dayRect(contentRect.left(), contentRect.top(), dayBlockWidth, contentRect.height());
        QRectF infoRect(dayRect.right() + padding, contentRect.top(), contentRect.width() - dayBlockWidth - padding, contentRect.height());

        QFont baseFont = painter.font();
        QFont dayFont = baseFont;
        dayFont.setBold(true);
        dayFont.setPointSizeF(dayFont.pointSizeF() * 1.5);
        painter.setFont(dayFont);
        painter.drawText(dayRect, Qt::AlignLeft | Qt::AlignVCenter, dayText);

        QFont infoFont = baseFont;
        infoFont.setBold(false);
        painter.setFont(infoFont);
        QRectF weekdayRect(infoRect.left(), infoRect.top(), infoRect.width(), infoRect.height() / 2);
        QRectF monthRect(infoRect.left(), infoRect.center().y(), infoRect.width(), infoRect.height() / 2);
        painter.drawText(weekdayRect, Qt::AlignLeft | Qt::AlignVCenter, weekdayText);
        painter.drawText(monthRect, Qt::AlignLeft | Qt::AlignVCenter, monthYearText);

        if (isToday) {
            highlightLines[x] = QColor(0, 150, 0);
            highlightLines[x + m_dayWidth] = QColor(0, 150, 0);
        }
        if (date.day() == 1 && highlightLines.find(x) == highlightLines.end()) {
            highlightLines[x] = Qt::black;
        }
    }

    painter.setPen(palette().dark().color());

    const double clipHeight = qMax(0.0, static_cast<double>(viewport()->height()) - m_headerHeight);
    painter.save();
    painter.setClipRect(QRectF(0, m_headerHeight, viewport()->width(), clipHeight));

    painter.setPen(palette().mid().color());
    for (int hour = 0; hour <= 24; ++hour) {
        const double y = bodyOriginY + hour * m_hourHeight;
        if (y < m_headerHeight - m_hourHeight || y > viewport()->height() + m_hourHeight) {
            continue;
        }
        painter.drawLine(QPointF(0, y), QPointF(totalWidth, y));
        painter.drawText(QRectF(0, y - m_hourHeight, m_timeAxisWidth - 4, m_hourHeight),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("%1:00").arg(hour, 2, 10, QLatin1Char('0')));
    }

    painter.setPen(palette().dark().color());
    for (int day = 0; day < m_dayCount; ++day) {
        const double x = m_timeAxisWidth + day * m_dayWidth;
        painter.drawRect(QRectF(x, bodyOriginY, m_dayWidth, bodyHeight));
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF visibleRect(m_timeAxisWidth,
                             m_headerHeight - m_hourHeight,
                             qMax(0.0, static_cast<double>(viewport()->width()) - m_timeAxisWidth),
                             viewport()->height() - m_headerHeight + m_hourHeight * 2);
    for (const auto &eventData : m_events) {
        QRectF rect = eventRect(eventData);
        rect.translate(0, -yOffset);
        if (!rect.intersects(visibleRect)) {
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
        QRectF previewRect = m_dropPreviewRect.translated(0, -yOffset);
        painter.setBrush(QColor(100, 149, 237, 120));
        painter.setPen(QPen(palette().highlight().color(), 1, Qt::DashLine));
        painter.drawRoundedRect(previewRect, 4, 4);
        painter.setPen(Qt::white);
        painter.drawText(previewRect.adjusted(4, 2, -4, -2),
                         Qt::AlignLeft | Qt::AlignTop,
                         m_dropPreviewText);
    }

    painter.restore();

    if (!highlightLines.empty()) {
        for (const auto &[lineX, color] : highlightLines) {
            QPen highlightPen(color, 3);
            painter.setPen(highlightPen);
            painter.drawLine(QPointF(lineX, 0), QPointF(lineX, viewport()->height()));
        }
    }
}

void CalendarView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    recalculateDayWidth();
    updateScrollBars();
}

void CalendarView::wheelEvent(QWheelEvent *event)
{
    if (handleWheelInteraction(event)) {
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
        cancelNewEventDrag();
        for (const auto &ev : m_events) {
            const QRectF rect = eventRect(ev);
            const bool withinX = scenePos.x() >= rect.left() && scenePos.x() <= rect.right();
            if (withinX) {
                auto topArea = handleArea(ev, true);
                if (scenePos.y() >= topArea.first && scenePos.y() <= topArea.second) {
                    beginResize(ev, true);
                    m_newEventDragPending = false;
                    m_newEventAnchorTime = QDateTime();
                    event->accept();
                    return;
                }
                auto bottomArea = handleArea(ev, false);
                if (scenePos.y() >= bottomArea.first && scenePos.y() <= bottomArea.second) {
                    beginResize(ev, false);
                    m_newEventDragPending = false;
                    m_newEventAnchorTime = QDateTime();
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
            m_newEventDragPending = false;
            m_newEventAnchorTime = QDateTime();
            event->accept();
            return;
        }
        m_selectedEvent = {};
        viewport()->update();
        emit selectionCleared();
        auto dateTimeOpt = dateTimeAtScene(scenePos);
        if (dateTimeOpt) {
            QDateTime anchor = dateTimeOpt.value();
            int minutes = snapMinutes(anchor.time().hour() * 60 + anchor.time().minute());
            anchor.setTime(QTime(minutes / 60, minutes % 60));
            m_newEventAnchorTime = anchor;
            m_newEventDragPending = true;
        } else {
            m_newEventDragPending = false;
            m_newEventAnchorTime = QDateTime();
        }
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
    const QPointF scenePos = QPointF(event->pos())
        + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    if ((event->buttons() & Qt::LeftButton) && !m_dragCandidateId.isNull()) {
        if ((event->pos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            auto it = std::find_if(m_events.begin(), m_events.end(), [this](const data::CalendarEvent &ev) {
                return ev.id == m_dragCandidateId;
            });
            if (it != m_events.end()) {
                beginInternalEventDrag(*it, m_dragPointerOffsetMinutes);
                updateInternalEventDrag(scenePos);
                event->accept();
                return;
            }
            resetDragCandidate();
        }
    }
    if (m_internalDragActive && (event->buttons() & Qt::LeftButton)) {
        updateInternalEventDrag(scenePos);
        event->accept();
        return;
    }
    if ((event->buttons() & Qt::LeftButton) && m_newEventDragPending) {
        if (!m_newEventDragActive
            && (event->pos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            startNewEventDrag();
        }
        if (m_newEventDragActive) {
            updateNewEventDrag(scenePos);
            event->accept();
            return;
        }
    } else if (m_newEventDragActive && !(event->buttons() & Qt::LeftButton)) {
        cancelNewEventDrag();
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
    if (m_internalDragActive && event->button() == Qt::LeftButton) {
        const QPointF scenePos = QPointF(event->pos())
            + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
        finalizeInternalEventDrag(scenePos);
        event->accept();
        return;
    }
    if (m_newEventDragActive && event->button() == Qt::LeftButton) {
        const QPointF scenePos = QPointF(event->pos())
            + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
        updateNewEventDrag(scenePos);
        finalizeNewEventDrag();
        event->accept();
        return;
    }
    if (m_newEventDragPending && event->button() == Qt::LeftButton) {
        cancelNewEventDrag();
    }
    resetDragCandidate();
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void CalendarView::dragEnterEvent(QDragEnterEvent *event)
{
    m_dragInteractionActive = true;
    m_autoScrollTimer.invalidate();
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
    maybeAutoScrollHorizontally(event->pos());
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
    m_dragInteractionActive = false;
    m_autoScrollTimer.invalidate();
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
    m_autoScrollTimer.invalidate();
    m_dragInteractionActive = false;
    Q_UNUSED(event);
    clearDropPreview();
}

bool CalendarView::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    if (!m_dragInteractionActive) {
        return QObject::eventFilter(watched, event);
    }
    switch (event->type()) {
    case QEvent::Wheel: {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (handleWheelInteraction(wheel)) {
            return true;
        }
        break;
    }
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        if (m_forwardingKeyEvent) {
            break;
        }
        auto *key = static_cast<QKeyEvent *>(event);
        QWidget *target = window();
        if (!target) {
            target = this;
        }
        QKeyEvent clone(event->type(),
                        key->key(),
                        key->modifiers(),
                        key->text(),
                        key->isAutoRepeat(),
                        key->count());
        m_forwardingKeyEvent = true;
        QCoreApplication::sendEvent(target, &clone);
        m_forwardingKeyEvent = false;
        return clone.isAccepted();
    }
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
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

void CalendarView::clearExternalSelection()
{
    m_selectedEvent = QUuid();
    viewport()->update();
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
    const double width = qMax(0.0, m_dayWidth - 12);

    return QRectF(x, y, width, height);
}

void CalendarView::updateScrollBars()
{
    const double bodyHeight = m_hourHeight * 24.0;
    const int pageStep = qMax(1, static_cast<int>(viewport()->height() - m_headerHeight));
    horizontalScrollBar()->setRange(0, 0);
    horizontalScrollBar()->setPageStep(viewport()->width());
    horizontalScrollBar()->setValue(0);

    verticalScrollBar()->setRange(0, qMax(0, static_cast<int>(bodyHeight - pageStep)));
    verticalScrollBar()->setPageStep(pageStep);
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
    if (m_dayWidth <= 0.0) {
        return;
    }
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
    if (m_dayWidth <= 0.0) {
        return std::nullopt;
    }
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

void CalendarView::beginInternalEventDrag(const data::CalendarEvent &event, int pointerOffsetMinutes)
{
    m_internalDragActive = true;
    m_internalDragSource = event;
    m_internalDragOffsetMinutes = pointerOffsetMinutes;
    m_internalDragDurationMinutes = qMax(15, static_cast<int>(event.start.secsTo(event.end) / 60));
    m_dragInteractionActive = true;
}

void CalendarView::updateInternalEventDrag(const QPointF &scenePos)
{
    if (!m_internalDragActive) {
        return;
    }
    auto dateTimeOpt = dateTimeAtScene(scenePos);
    if (!dateTimeOpt) {
        clearDropPreview();
        return;
    }
    QDateTime target = dateTimeOpt.value();
    int minutes = target.time().hour() * 60 + target.time().minute();
    minutes = snapMinutes(minutes);
    target.setTime(QTime(minutes / 60, minutes % 60));
    target = target.addSecs(-m_internalDragOffsetMinutes * 60);
    minutes = target.time().hour() * 60 + target.time().minute();
    minutes = snapMinutes(minutes);
    target.setTime(QTime(minutes / 60, minutes % 60));
    updateDropPreview(target, m_internalDragDurationMinutes, m_internalDragSource.title);
}

void CalendarView::finalizeInternalEventDrag(const QPointF &scenePos)
{
    if (!m_internalDragActive) {
        return;
    }
    const QPoint globalPos = QCursor::pos();
    if (cursorOverTodoList(globalPos)) {
        emit eventDroppedToTodo(m_internalDragSource);
        cancelInternalEventDrag();
        return;
    }
    auto dateTimeOpt = dateTimeAtScene(scenePos);
    if (dateTimeOpt) {
        QDateTime target = dateTimeOpt.value();
        int minutes = target.time().hour() * 60 + target.time().minute();
        minutes = snapMinutes(minutes);
        target.setTime(QTime(minutes / 60, minutes % 60));
        target = target.addSecs(-m_internalDragOffsetMinutes * 60);
        minutes = target.time().hour() * 60 + target.time().minute();
        minutes = snapMinutes(minutes);
        target.setTime(QTime(minutes / 60, minutes % 60));
        bool copy = QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
        emit eventDropRequested(m_internalDragSource.id, target, copy);
    }
    cancelInternalEventDrag();
}

void CalendarView::cancelInternalEventDrag()
{
    if (!m_internalDragActive) {
        return;
    }
    m_internalDragActive = false;
    m_internalDragDurationMinutes = 0;
    m_dragInteractionActive = false;
    clearDropPreview();
    resetDragCandidate();
}

bool CalendarView::cursorOverTodoList(const QPoint &globalPos) const
{
    QWidget *widget = QApplication::widgetAt(globalPos);
    while (widget) {
        if (widget->objectName() == QLatin1String("todoList")) {
            return true;
        }
    widget = widget->parentWidget();
    }
    return false;
}

void CalendarView::startNewEventDrag()
{
    if (!m_newEventDragPending || !m_newEventAnchorTime.isValid()) {
        return;
    }
    m_newEventDragActive = true;
    m_dragInteractionActive = true;
    m_newEventStart = m_newEventAnchorTime;
    m_newEventEnd = m_newEventAnchorTime.addSecs(SnapIntervalMinutes * 60);
    const QString label = tr("Neuer Termin\n%1 - %2")
                              .arg(m_newEventStart.time().toString(QStringLiteral("hh:mm")),
                                   m_newEventEnd.time().toString(QStringLiteral("hh:mm")));
    updateDropPreview(m_newEventStart,
                      qMax(SnapIntervalMinutes,
                           static_cast<int>(m_newEventStart.secsTo(m_newEventEnd) / 60)),
                      label);
}

void CalendarView::updateNewEventDrag(const QPointF &scenePos)
{
    if (!m_newEventDragActive) {
        return;
    }
    auto dateTimeOpt = dateTimeAtScene(scenePos);
    QDateTime current = dateTimeOpt.value_or(m_newEventAnchorTime);
    if (current.isValid()) {
        int minutes = snapMinutes(current.time().hour() * 60 + current.time().minute());
        current.setTime(QTime(minutes / 60, minutes % 60));
    } else {
        current = m_newEventAnchorTime;
    }
    if (!m_newEventAnchorTime.isValid()) {
        return;
    }
    QDateTime start = m_newEventAnchorTime;
    QDateTime end = current;
    if (end < start) {
        std::swap(start, end);
    }
    if (!end.isValid() || end <= start) {
        end = start.addSecs(SnapIntervalMinutes * 60);
    }
    const int durationMinutes = qMax(SnapIntervalMinutes,
                                     static_cast<int>(start.secsTo(end) / 60));
    m_newEventStart = start;
    m_newEventEnd = start.addSecs(durationMinutes * 60);
    const QString label = tr("Neuer Termin\n%1 - %2")
                              .arg(m_newEventStart.time().toString(QStringLiteral("hh:mm")),
                                   m_newEventEnd.time().toString(QStringLiteral("hh:mm")));
    updateDropPreview(m_newEventStart, durationMinutes, label);
}

void CalendarView::finalizeNewEventDrag()
{
    if (!m_newEventDragActive) {
        cancelNewEventDrag();
        return;
    }
    clearDropPreview();
    m_dragInteractionActive = false;
    m_newEventDragActive = false;
    m_newEventDragPending = false;
    if (m_newEventStart.isValid() && m_newEventEnd.isValid() && m_newEventEnd > m_newEventStart) {
        emit eventCreationRequested(m_newEventStart, m_newEventEnd);
    }
    m_newEventAnchorTime = QDateTime();
    m_newEventStart = QDateTime();
    m_newEventEnd = QDateTime();
}

void CalendarView::cancelNewEventDrag()
{
    if (m_newEventDragActive) {
        clearDropPreview();
        m_dragInteractionActive = false;
    }
    m_newEventDragActive = false;
    m_newEventDragPending = false;
    m_newEventAnchorTime = QDateTime();
    m_newEventStart = QDateTime();
    m_newEventEnd = QDateTime();
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

void CalendarView::recalculateDayWidth()
{
    const double availableWidth = qMax(0.0, static_cast<double>(viewport()->width()) - m_timeAxisWidth);
    const int days = qMax(1, m_dayCount);
    const double newWidth = days > 0 ? availableWidth / static_cast<double>(days) : availableWidth;
    if (!qFuzzyCompare(1.0 + m_dayWidth, 1.0 + newWidth)) {
        m_dayWidth = newWidth;
        viewport()->update();
    }
}

void CalendarView::maybeAutoScrollHorizontally(const QPoint &pos)
{
    constexpr int Margin = 40;
    constexpr int IntervalMs = 120;
    if (!m_autoScrollTimer.isValid()) {
        m_autoScrollTimer.start();
    }
    if (m_autoScrollTimer.elapsed() < IntervalMs) {
        return;
    }
    if (pos.x() <= Margin) {
        emit dayScrollRequested(-1);
        m_autoScrollTimer.restart();
    } else if (pos.x() >= viewport()->width() - Margin) {
        emit dayScrollRequested(1);
        m_autoScrollTimer.restart();
    }
}

bool CalendarView::handleWheelInteraction(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const int delta = event->angleDelta().y();
        if (delta != 0) {
            emit dayZoomRequested(delta > 0);
        }
        event->accept();
        return true;
    }
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        zoomTime(event->angleDelta().y() > 0 ? 1.1 : 0.9);
        event->accept();
        return true;
    }
    const QPoint angle = event->angleDelta();
    bool handled = false;
    if (angle.x() != 0) {
        m_horizontalScrollRemainder += static_cast<double>(angle.x()) / 120.0;
        while (m_horizontalScrollRemainder >= 1.0) {
            emit dayScrollRequested(-1);
            m_horizontalScrollRemainder -= 1.0;
            handled = true;
        }
        while (m_horizontalScrollRemainder <= -1.0) {
            emit dayScrollRequested(1);
            m_horizontalScrollRemainder += 1.0;
            handled = true;
        }
    }
    if (angle.y() != 0) {
        const double steps = angle.y() / 120.0;
        const double delta = steps * (m_hourHeight * 0.5); // 30 Minuten
        auto *vbar = verticalScrollBar();
        const int newValue = qBound(vbar->minimum(), vbar->value() - static_cast<int>(delta), vbar->maximum());
        vbar->setValue(newValue);
        handled = true;
    }
    if (handled) {
        event->accept();
        return true;
    }
    return false;
}

} // namespace ui
} // namespace calendar
