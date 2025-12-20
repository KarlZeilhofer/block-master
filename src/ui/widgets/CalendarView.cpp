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
#include <QPainterPath>
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

#include "calendar/ui/mime/TodoMime.hpp"

namespace calendar {
namespace ui {

namespace {
constexpr double MinHourHeight = 20.0;
constexpr double MaxHourHeight = 160.0;
constexpr double HandleZone = 8.0;
constexpr int SnapIntervalMinutes = 15;
const char *EventMimeType = "application/x-calendar-event";
constexpr double HandleHoverRange = 12.0;
constexpr double EventCornerRadius = 8.0;
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
    m_allowNewEventCreation = true;
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
    const auto segmentPath = [](const QRectF &rect, bool clipTop, bool clipBottom) {
        QPainterPath path;
        const double radius = qMin(EventCornerRadius, qMin(rect.width(), rect.height()) / 2.0);
        path.addRoundedRect(rect, radius, radius);
        if (clipTop) {
            path.addRect(QRectF(rect.left(), rect.top(), rect.width(), radius));
        }
        if (clipBottom) {
            path.addRect(QRectF(rect.left(), rect.bottom() - radius, rect.width(), radius));
        }
        path.setFillRule(Qt::WindingFill);
        return path;
    };

    for (const auto &eventData : m_events) {
        const auto segments = segmentsForEvent(eventData);
        if (segments.empty()) {
            continue;
        }

        QColor color = palette().highlight().color();
        if (eventData.id == m_selectedEvent) {
            color = color.darker(125);
        }
        const qint64 durationMinutes = qMax<qint64>(5, eventData.start.secsTo(eventData.end) / 60);
        const int hours = static_cast<int>(durationMinutes / 60);
        const int minutes = static_cast<int>(durationMinutes % 60);
        QString durationText;
        if (hours > 0 && minutes > 0) {
            durationText = tr("%1h %2m").arg(hours).arg(minutes);
        } else if (hours > 0) {
            durationText = tr("%1h").arg(hours);
        } else {
            durationText = tr("%1m").arg(minutes);
        }
        const QString startInfo = tr("%1 (%2)")
                                      .arg(eventData.start.time().toString(QStringLiteral("hh:mm")),
                                           durationText);
        const QString titleBlock = tr("%1\n%2").arg(eventData.title, startInfo);
        bool infoDrawn = false;

        const double handleHeight = 6.0;
        QColor handleColor = palette().highlight().color().lighter(130);
        handleColor.setAlpha(160);

        QRectF topRect = segments.front().rect;
        QRectF bottomRect = segments.back().rect;

        for (const auto &segment : segments) {
            QRectF rect = segment.rect.translated(0, -yOffset);
            if (!rect.intersects(visibleRect)) {
                continue;
            }
            QPainterPath path = segmentPath(rect, segment.clipTop, segment.clipBottom);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawPath(path);

            painter.setPen(Qt::white);
            QString block;
            if (segment.clipTop) {
                block = tr("... %1").arg(eventData.title);
            } else if (!infoDrawn) {
                block = titleBlock;
            } else {
                block = eventData.title;
            }
            QRectF textRect = rect.adjusted(4, 2, -4, -rect.height() / 2);
            painter.drawText(textRect,
                             Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                             block);
            infoDrawn = true;

            const QString endLabel = segment.clipBottom
                ? QStringLiteral("...")
                : segment.segmentEnd.time().toString(QStringLiteral("hh:mm"));
            painter.drawText(QRectF(rect.left() + 4,
                                    rect.bottom() - 20,
                                    rect.width() - 8,
                                    18),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             endLabel);
        }

        painter.setPen(Qt::NoPen);
        if (eventData.id == m_hoverTopHandleId) {
            QRectF translatedTop = topRect.translated(0, -yOffset);
            if (translatedTop.intersects(visibleRect)) {
                QRectF topHandle(translatedTop.x() + 6, translatedTop.y() - handleHeight / 2, translatedTop.width() - 12, handleHeight);
                painter.setBrush(handleColor);
                painter.drawRoundedRect(topHandle, 3, 3);
                painter.setPen(QPen(palette().base().color(), 1));
                painter.drawLine(QPointF(topHandle.left() + 4, topHandle.center().y()),
                                 QPointF(topHandle.right() - 4, topHandle.center().y()));
                painter.setPen(Qt::NoPen);
            }
        }
        if (eventData.id == m_hoverBottomHandleId) {
            QRectF translatedBottom = bottomRect.translated(0, -yOffset);
            if (translatedBottom.intersects(visibleRect)) {
                QRectF bottomHandle(translatedBottom.x() + 6, translatedBottom.bottom() - handleHeight / 2, translatedBottom.width() - 12, handleHeight);
                painter.setBrush(handleColor);
                painter.drawRoundedRect(bottomHandle, 3, 3);
                painter.setPen(QPen(palette().base().color(), 1));
                painter.drawLine(QPointF(bottomHandle.left() + 4, bottomHandle.center().y()),
                                 QPointF(bottomHandle.right() - 4, bottomHandle.center().y()));
                painter.setPen(Qt::NoPen);
            }
        }
    }

    if (m_showDropPreview) {
        const auto segments = segmentsForEvent(m_dropPreviewEvent);
        bool labelDrawn = false;
        for (const auto &segment : segments) {
            QRectF rect = segment.rect.translated(0, -yOffset);
            if (!rect.intersects(visibleRect)) {
                continue;
            }
            painter.setBrush(QColor(100, 149, 237, 120));
            painter.setPen(QPen(palette().highlight().color(), 1, Qt::DashLine));
            QPainterPath path = segmentPath(rect, segment.clipTop, segment.clipBottom);
            painter.drawPath(path);
            if (!labelDrawn) {
                painter.setPen(Qt::white);
                painter.drawText(rect.adjusted(4, 2, -4, -2),
                                 Qt::AlignLeft | Qt::AlignTop,
                                 m_dropPreviewText);
                labelDrawn = true;
            }
        }
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
            QDateTime dateTime = snapDateTime(dateTimeOpt.value());
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
            const auto segments = segmentsForEvent(ev);
            if (segments.empty()) {
                continue;
            }
            const QRectF &topRect = segments.front().rect;
            if (scenePos.x() >= topRect.left() && scenePos.x() <= topRect.right()) {
                auto topArea = handleArea(ev, true);
                if (scenePos.y() >= topArea.first && scenePos.y() <= topArea.second) {
                    beginResize(ev, true);
                    m_newEventDragPending = false;
                    m_newEventAnchorTime = QDateTime();
                    event->accept();
                    return;
                }
            }
            const QRectF &bottomRect = segments.back().rect;
            if (scenePos.x() >= bottomRect.left() && scenePos.x() <= bottomRect.right()) {
                auto bottomArea = handleArea(ev, false);
                if (scenePos.y() >= bottomArea.first && scenePos.y() <= bottomArea.second) {
                    beginResize(ev, false);
                    m_newEventDragPending = false;
                    m_newEventAnchorTime = QDateTime();
                    event->accept();
                    return;
                }
            }

            bool contains = false;
            for (const auto &segment : segments) {
                if (segment.rect.contains(scenePos)) {
                    contains = true;
                    break;
                }
            }
            if (!contains) {
                continue;
            }
            m_dragCandidateId = ev.id;
            int rawOffset = 0;
            if (const auto dateTimeOpt = dateTimeAtScene(scenePos)) {
                rawOffset = static_cast<int>(ev.start.secsTo(dateTimeOpt.value()) / 60);
            }
            rawOffset = snapIntervalMinutes(rawOffset);
            const int durationMinutes = qMax(1, static_cast<int>(ev.start.secsTo(ev.end) / 60));
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
        clearDropPreview();
        if (!m_allowNewEventCreation) {
            event->accept();
            return;
        }
        auto dateTimeOpt = dateTimeAtScene(scenePos);
        if (dateTimeOpt) {
            QDateTime anchor = snapDateTime(dateTimeOpt.value());
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
        const auto entry = firstTodoMimeEntry(mime->data(TodoMimeType));
        if (!entry.has_value()) {
            clearDropPreview();
            event->ignore();
            return;
        }
        auto dateTimeOpt = dateTimeAtScene(scenePos);
        if (!dateTimeOpt) {
            clearDropPreview();
            event->ignore();
            return;
        }
        QDateTime dateTime = snapDateTime(dateTimeOpt.value());
        const int durationMinutes = entry->durationMinutes > 0 ? entry->durationMinutes : 60;
        const QString label = entry->title.isEmpty() ? tr("Neuer Termin") : entry->title;
        updateDropPreview(dateTime, durationMinutes, label);
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
        QDateTime dateTime = snapDateTime(dateTimeOpt.value());
        dateTime = dateTime.addSecs(-offsetMinutes * 60);
        dateTime = snapDateTime(dateTime);
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
        const auto entry = firstTodoMimeEntry(mime->data(TodoMimeType));
        if (!entry.has_value()) {
            event->ignore();
            return;
        }
        auto dateTimeOpt = dateTimeAtScene(scenePos);
        if (!dateTimeOpt.has_value()) {
            event->ignore();
            return;
        }
        QDateTime dateTime = snapDateTime(dateTimeOpt.value());
        emit todoDropped(entry->id, dateTime);
        m_allowNewEventCreation = false;
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
        QDateTime dateTime = snapDateTime(dateTimeOpt.value());
        dateTime = dateTime.addSecs(-offsetMinutes * 60);
        dateTime = snapDateTime(dateTime);
        bool copy = event->keyboardModifiers().testFlag(Qt::ControlModifier);
        emit eventDropRequested(eventId, dateTime, copy);
        m_allowNewEventCreation = false;
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

void CalendarView::clearGhostPreview()
{
    clearDropPreview();
}

std::vector<CalendarView::EventSegment> CalendarView::segmentsForEvent(const data::CalendarEvent &event) const
{
    std::vector<EventSegment> segments;
    if (!event.start.isValid() || !event.end.isValid() || event.start >= event.end || m_dayWidth <= 0.0) {
        return segments;
    }
    for (int day = 0; day < m_dayCount; ++day) {
        const QDate date = m_startDate.addDays(day);
        const QDateTime dayStart(date, QTime(0, 0));
        const QDateTime dayEnd = dayStart.addDays(1);
        if (event.end <= dayStart || event.start >= dayEnd) {
            continue;
        }
        QDateTime segmentStart = event.start > dayStart ? event.start : dayStart;
        QDateTime segmentEnd = event.end < dayEnd ? event.end : dayEnd;
        const double startMinutes = dayStart.secsTo(segmentStart) / 60.0;
        const double endMinutes = dayStart.secsTo(segmentEnd) / 60.0;
        const double y = m_headerHeight + (startMinutes / 60.0) * m_hourHeight;
        const double durationMinutes = qMax(0.0, endMinutes - startMinutes);
        const double height = qMax((durationMinutes / 60.0) * m_hourHeight, 20.0);
        const double x = m_timeAxisWidth + day * m_dayWidth + 6.0;
        const double width = qMax(0.0, m_dayWidth - 12.0);

        EventSegment segment;
        segment.rect = QRectF(x, y, width, height);
        segment.segmentStart = segmentStart;
        segment.segmentEnd = segmentEnd;
        segment.clipTop = segmentStart > event.start;
        segment.clipBottom = segmentEnd < event.end;
        segments.push_back(segment);
    }
    return segments;
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
        const auto segments = segmentsForEvent(eventData);
        for (const auto &segment : segments) {
            if (segment.rect.contains(scenePos)) {
                m_selectedEvent = eventData.id;
                viewport()->update();
                emit eventActivated(eventData);
                emit eventSelected(eventData);
                return;
            }
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
        const auto segments = segmentsForEvent(eventData);
        if (segments.empty()) {
            continue;
        }
        const QRectF &topRect = segments.front().rect;
        if (scenePos.x() >= topRect.left() && scenePos.x() <= topRect.right()) {
            auto topArea = handleArea(eventData, true);
            if (scenePos.y() >= topArea.first && scenePos.y() <= topArea.second) {
                newTop = eventData.id;
            }
        }

        const QRectF &bottomRect = segments.back().rect;
        if (scenePos.x() >= bottomRect.left() && scenePos.x() <= bottomRect.right()) {
            auto bottomArea = handleArea(eventData, false);
            if (scenePos.y() >= bottomArea.first && scenePos.y() <= bottomArea.second) {
                newBottom = eventData.id;
            }
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

QDateTime CalendarView::snapDateTime(const QDateTime &value) const
{
    if (!value.isValid()) {
        return value;
    }
    QDateTime snapped = value;
    const int minutes = value.time().hour() * 60 + value.time().minute();
    int rounded = snapMinutes(minutes);
    if (rounded >= 24 * 60) {
        snapped = snapped.addDays(1);
        snapped.setTime(QTime(0, 0));
    } else {
        snapped.setTime(QTime(rounded / 60, rounded % 60));
    }
    return snapped;
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
    const auto dateTimeOpt = dateTimeAtScene(scenePos);
    if (!dateTimeOpt) {
        return;
    }
    QDateTime snapped = snapDateTime(dateTimeOpt.value());

    if (m_dragMode == DragMode::ResizeStart) {
        const auto minEnd = m_dragEvent.end.addSecs(-SnapIntervalMinutes * 60);
        QDateTime newStart = snapped;
        if (newStart < minEnd) {
            m_dragEvent.start = newStart;
        }
    } else if (m_dragMode == DragMode::ResizeEnd) {
        const auto minStart = m_dragEvent.start.addSecs(SnapIntervalMinutes * 60);
        QDateTime newEnd = snapped;
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
        const auto segments = segmentsForEvent(eventData);
        for (const auto &segment : segments) {
            if (segment.rect.contains(scenePos)) {
                return &eventData;
            }
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
    clearTodoHoverFeedback();
}

void CalendarView::updateInternalEventDrag(const QPointF &scenePos)
{
    if (!m_internalDragActive) {
        return;
    }
    if (auto dateTimeOpt = dateTimeAtScene(scenePos)) {
        QDateTime target = snapDateTime(dateTimeOpt.value());
        target = target.addSecs(-m_internalDragOffsetMinutes * 60);
        target = snapDateTime(target);
        updateDropPreview(target, m_internalDragDurationMinutes, m_internalDragSource.title);
    } else {
        clearDropPreview();
    }
    updateTodoHoverFeedback();
}

void CalendarView::finalizeInternalEventDrag(const QPointF &scenePos)
{
    if (!m_internalDragActive) {
        return;
    }
    const QPoint globalPos = QCursor::pos();
    if (const auto status = todoStatusUnderCursor(globalPos)) {
        emit eventDroppedToTodo(m_internalDragSource, status.value());
        cancelInternalEventDrag();
        return;
    }
    auto dateTimeOpt = dateTimeAtScene(scenePos);
    if (dateTimeOpt) {
        QDateTime target = snapDateTime(dateTimeOpt.value());
        target = target.addSecs(-m_internalDragOffsetMinutes * 60);
        target = snapDateTime(target);
        bool copy = QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
        emit eventDropRequested(m_internalDragSource.id, target, copy);
        m_allowNewEventCreation = false;
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
    clearTodoHoverFeedback();
}

std::optional<data::TodoStatus> CalendarView::todoStatusUnderCursor(const QPoint &globalPos) const
{
    QWidget *widget = QApplication::widgetAt(globalPos);
    while (widget) {
        const QVariant value = widget->property("todoStatus");
        if (value.isValid()) {
            bool ok = false;
            int statusValue = value.toInt(&ok);
            if (ok) {
                return static_cast<data::TodoStatus>(statusValue);
            }
        }
        widget = widget->parentWidget();
    }
    return std::nullopt;
}

void CalendarView::updateTodoHoverFeedback()
{
    if (!m_internalDragActive) {
        return;
    }
    const auto status = todoStatusUnderCursor(QCursor::pos());
    if (status == m_currentTodoHoverStatus) {
        return;
    }
    m_currentTodoHoverStatus = status;
    if (status.has_value()) {
        clearDropPreview();
        emit todoHoverPreviewRequested(status.value(), m_internalDragSource);
    } else {
        emit todoHoverPreviewCleared();
    }
}

void CalendarView::clearTodoHoverFeedback()
{
    if (!m_currentTodoHoverStatus.has_value()) {
        return;
    }
    m_currentTodoHoverStatus.reset();
    emit todoHoverPreviewCleared();
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
        current = snapDateTime(current);
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
    m_dragInteractionActive = false;
    m_newEventDragActive = false;
    const bool validRange = m_newEventStart.isValid() && m_newEventEnd.isValid() && m_newEventEnd > m_newEventStart;
    if (validRange) {
        emit eventCreationRequested(m_newEventStart, m_newEventEnd);
    } else {
        clearDropPreview();
    }
    m_newEventDragPending = false;
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

    const auto previewSegments = segmentsForEvent(preview);
    if (previewSegments.empty()) {
        clearDropPreview();
        return;
    }

    m_dropPreviewEvent = preview;
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
    m_dropPreviewEvent = data::CalendarEvent();
    m_dropPreviewText.clear();
    viewport()->update();
}

QPair<double, double> CalendarView::handleArea(const data::CalendarEvent &event, bool top) const
{
    const auto segments = segmentsForEvent(event);
    if (segments.empty()) {
        return { 0.0, 0.0 };
    }
    const QRectF rect = top ? segments.front().rect : segments.back().rect;
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
        const auto otherSegments = segmentsForEvent(other);
        for (const auto &otherRect : otherSegments) {
            if (!overlapsHorizontally(otherRect.rect)) {
                continue;
            }
            if (top && otherRect.rect.bottom() <= rect.top()) {
                minY = std::max(minY, otherRect.rect.bottom());
            } else if (!top && otherRect.rect.top() >= rect.bottom()) {
                maxY = std::min(maxY, otherRect.rect.top());
            }
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
