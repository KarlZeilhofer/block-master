#include "calendar/ui/widgets/CalendarView.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTime>
#include <QLocale>
#include <QtMath>

namespace calendar {
namespace ui {

namespace {
constexpr double MinHourHeight = 20.0;
constexpr double MaxHourHeight = 160.0;
constexpr double MinDayWidth = 120.0;
constexpr double MaxDayWidth = 420.0;
} // namespace

CalendarView::CalendarView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setMouseTracking(true);
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

        painter.setPen(Qt::white);
        painter.drawText(rect.adjusted(4, 2, -4, -2),
                         Qt::TextWordWrap,
                         QStringLiteral("%1\n%2 - %3")
                             .arg(eventData.title,
                                  eventData.start.time().toString(QStringLiteral("hh:mm")),
                                  eventData.end.time().toString(QStringLiteral("hh:mm"))));
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
    QAbstractScrollArea::wheelEvent(event);
}

void CalendarView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        selectEventAt(event->pos());
    }
    QAbstractScrollArea::mousePressEvent(event);
}

void CalendarView::mouseMoveEvent(QMouseEvent *event)
{
    emitHoverAt(event->pos());
    QAbstractScrollArea::mouseMoveEvent(event);
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
            return;
        }
    }
    m_selectedEvent = {};
    viewport()->update();
}

void CalendarView::emitHoverAt(const QPoint &pos)
{
    const QPointF scenePos = QPointF(pos) + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const double y = scenePos.y() - m_headerHeight;
    if (y < 0) {
        return;
    }
    const double hours = y / m_hourHeight;
    const int hour = static_cast<int>(hours);
    const int minute = static_cast<int>((hours - hour) * 60);
    const double x = scenePos.x() - m_timeAxisWidth;
    const int dayIndex = static_cast<int>(x / m_dayWidth);
    if (dayIndex < 0 || dayIndex >= m_dayCount) {
        return;
    }
    QDateTime dt(m_startDate.addDays(dayIndex), QTime(hour, minute));
    emit hoveredDateTime(dt);
}

} // namespace ui
} // namespace calendar
