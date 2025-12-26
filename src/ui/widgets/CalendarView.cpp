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
#include <QTimer>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <limits>
#include <QFontMetricsF>
#include <QToolTip>
#include <QHelpEvent>
#include <QStringList>

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
constexpr int LargePlacementThresholdMinutes = 16 * 60;
constexpr int LargePlacementOffsetMinutes = 8 * 60;
const QStringList UltraShortMonths = {
    QStringLiteral("Jr"),
    QStringLiteral("Fb"),
    QStringLiteral("Mz"),
    QStringLiteral("Ap"),
    QStringLiteral("Ma"),
    QStringLiteral("Jn"),
    QStringLiteral("Jl"),
    QStringLiteral("Ag"),
    QStringLiteral("Sp"),
    QStringLiteral("Ok"),
    QStringLiteral("Nv"),
    QStringLiteral("Dz")
};

int placementOffsetMinutes(int durationMinutes)
{
    if (durationMinutes <= 0) {
        return 0;
    }
    if (durationMinutes > LargePlacementThresholdMinutes) {
        return LargePlacementOffsetMinutes;
    }
    return durationMinutes / 2;
}

QString cleanMonthToken(const QString &token)
{
    QString result = token;
    return result.replace(QLatin1Char('.'), QString());
}

QString monthUltraShort(int month)
{
    if (month >= 1 && month <= UltraShortMonths.size()) {
        return UltraShortMonths.at(month - 1);
    }
    return QStringLiteral("??");
}

QString chooseLabel(const QStringList &variants, const QFontMetrics &metrics, double maxWidth)
{
    const int pixelWidth = qMax(0, static_cast<int>(std::floor(maxWidth)));
    for (const auto &variant : variants) {
        if (metrics.horizontalAdvance(variant) <= pixelWidth) {
            return variant;
        }
    }
    if (variants.isEmpty()) {
        return {};
    }
    return metrics.elidedText(variants.last(), Qt::ElideRight, pixelWidth);
}

QString weekdayForWidth(const QDate &date, const QFontMetrics &metrics, double width)
{
    const QString weekday = QLocale().toString(date, QStringLiteral("dddd"));
    QStringList variants;
    variants << weekday;
    const QString four = weekday.left(4);
    if (!variants.contains(four)) {
        variants << four;
    }
    const QString two = weekday.left(2);
    if (!variants.contains(two)) {
        variants << two;
    }
    return chooseLabel(variants, metrics, width);
}

QString monthYearForWidth(const QDate &date, const QFontMetrics &metrics, double width)
{
    const QString longForm = QLocale().toString(date, QStringLiteral("MMMM yyyy"));
    QString shortMonth = cleanMonthToken(QLocale().toString(date, QStringLiteral("MMM")));
    const QString medium = QStringLiteral("%1 %2").arg(shortMonth, date.toString(QStringLiteral("yy")));
    const QString compact = QStringLiteral("%1%2").arg(monthUltraShort(date.month()),
                                                      date.toString(QStringLiteral("yy")));
    QStringList variants;
    variants << longForm << medium << compact;
    return chooseLabel(variants, metrics, width);
}

QFont fitFontToHeight(const QFont &base, double availableHeight, double multiplier = 1.0)
{
    if (availableHeight <= 0.0) {
        return base;
    }
    QFont font = base;
    double baseSize = base.pointSizeF();
    if (baseSize <= 0.0) {
        if (base.pixelSize() > 0) {
            baseSize = static_cast<double>(base.pixelSize());
        } else {
            baseSize = 12.0;
        }
    }
    double size = baseSize * multiplier;
    font.setPointSizeF(size);
    QFontMetricsF metrics(font);
    if (metrics.height() > availableHeight && metrics.height() > 0.0) {
        double scale = availableHeight / metrics.height();
        font.setPointSizeF(qMax(1.0, size * scale));
    }
    return font;
}
} // namespace

CalendarView::CalendarView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
    invalidateLayout();
    recalculateDayWidth();
    viewport()->update();
    updateScrollBars();
    refreshActiveDragPreview();
}

void CalendarView::setDayOffset(double offsetDays)
{
    double normalized = std::fmod(offsetDays, 1.0);
    if (normalized < 0.0) {
        normalized += 1.0;
    }
    if (normalized < 0.0 || normalized >= 1.0) {
        normalized = 0.0;
    }
    if (qFuzzyCompare(1.0 + m_dayOffset, 1.0 + normalized)) {
        return;
    }
    m_dayOffset = normalized;
    invalidateLayout();
    viewport()->update();
    refreshActiveDragPreview();
}

void CalendarView::setEvents(std::vector<data::CalendarEvent> events)
{
    m_events = std::move(events);
    m_allowNewEventCreation = true;
    invalidateLayout();
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

void CalendarView::setHourHeight(double height)
{
    m_hourHeight = qBound(MinHourHeight, height, MaxHourHeight);
    updateScrollBars();
    viewport()->update();
}

int CalendarView::verticalScrollValue() const
{
    if (auto *vbar = verticalScrollBar()) {
        return vbar->value();
    }
    return 0;
}

void CalendarView::setVerticalScrollValue(int value)
{
    if (auto *vbar = verticalScrollBar()) {
        vbar->setValue(qBound(vbar->minimum(), value, vbar->maximum()));
    }
}

void CalendarView::setEventSearchFilter(const QString &text)
{
    QString normalized = text.trimmed();
    if (m_eventSearchFilter == normalized) {
        return;
    }
    m_eventSearchFilter = normalized;
    viewport()->update();
}

void CalendarView::temporarilyDisableNewEventCreation()
{
    m_allowNewEventCreation = false;
    QTimer::singleShot(0, this, [this]() {
        m_allowNewEventCreation = true;
    });
}

bool CalendarView::eventMatchesFilter(const data::CalendarEvent &event) const
{
    if (m_eventSearchFilter.isEmpty()) {
        return true;
    }
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    if (event.title.contains(m_eventSearchFilter, cs)) {
        return true;
    }
    if (event.description.contains(m_eventSearchFilter, cs)) {
        return true;
    }
    if (event.location.contains(m_eventSearchFilter, cs)) {
        return true;
    }
    const QString timeString = QLocale().toString(event.start, QLocale::ShortFormat);
    if (timeString.contains(m_eventSearchFilter, cs)) {
        return true;
    }
    return false;
}

bool CalendarView::showMonthBand() const
{
    return m_dayWidth < 75.0;
}

double CalendarView::monthBandHeight() const
{
    return showMonthBand() ? m_headerHeight : 0.0;
}

double CalendarView::totalHeaderHeight() const
{
    return m_headerHeight + monthBandHeight();
}

void CalendarView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    ensureLayoutCache();
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), palette().base());

    const double yOffset = verticalScrollBar()->value();
    const double bodyHeight = m_hourHeight * 24.0;
    const double monthBandHeight = this->monthBandHeight();
    const double totalHeaderHeight = this->totalHeaderHeight();
    const double bodyOriginY = totalHeaderHeight - yOffset;
    const double totalWidth = contentRightEdge();
    const int daySlots = daySlotCount();

    // Header background (sticky)
    painter.fillRect(QRectF(0, 0, viewport()->width(), totalHeaderHeight), palette().alternateBase());
    painter.setPen(palette().dark().color());
    painter.drawLine(QPointF(0, totalHeaderHeight - 0.5), QPointF(viewport()->width(), totalHeaderHeight - 0.5));

    const QFont originalFont = painter.font();
    if (showMonthBand()) {
        QFont monthFont = originalFont;
        monthFont.setBold(true);
        double baseSize = monthFont.pointSizeF();
        if (baseSize <= 0.0) {
            baseSize = monthFont.pixelSize() > 0 ? monthFont.pixelSize() : 12.0;
        }
        monthFont.setPointSizeF(baseSize * 2.0);
        painter.setFont(monthFont);
        painter.setPen(Qt::black);
        for (int day = 0; day < daySlots;) {
            const QDate date = m_startDate.addDays(day);
            int span = 1;
            while (day + span < daySlots && m_startDate.addDays(day + span).month() == date.month()) {
                ++span;
            }
            const double startX = dayColumnLeft(day);
            const double spanWidth = span * m_dayWidth;
            QRectF monthRect(startX, 0.0, spanWidth, monthBandHeight);
            painter.drawText(monthRect, Qt::AlignCenter | Qt::AlignVCenter, QLocale().toString(date, QStringLiteral("MMMM yyyy")));
            day += span;
        }
        painter.setPen(palette().windowText().color());
        painter.setFont(originalFont);
    }

    std::map<double, QColor> highlightLines;
    painter.setPen(palette().dark().color());
    painter.drawLine(QPointF(m_timeAxisWidth, monthBandHeight),
                     QPointF(contentRightEdge(), monthBandHeight));
    const QDate today = QDate::currentDate();

    for (int day = 0; day < daySlots; ++day) {
        const double x = dayColumnLeft(day);
        QRectF headerRect(x, monthBandHeight, m_dayWidth, m_headerHeight);
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

        const double padding = 6.0;
        QRectF contentRect = headerRect.adjusted(padding, padding / 2, -padding, -padding / 2);

        const bool compactHeader = showMonthBand();
        QFont baseFont = painter.font();
        QFont boldFont = baseFont;
        boldFont.setBold(true);

        if (compactHeader) {
            QRectF weekdayRect(contentRect.left(),
                               contentRect.top(),
                               contentRect.width(),
                               contentRect.height() * 0.35);
            QRectF dayRect(contentRect.left(),
                           weekdayRect.bottom(),
                           contentRect.width(),
                           contentRect.height() - weekdayRect.height());

            QFont weekdayFont = fitFontToHeight(baseFont, weekdayRect.height(), 0.85);
            painter.setFont(weekdayFont);
            QFontMetrics weekdayMetrics(weekdayFont);
            const QString weekdayDisplay = weekdayForWidth(date, weekdayMetrics, weekdayRect.width());
            painter.drawText(weekdayRect, Qt::AlignLeft | Qt::AlignBottom, weekdayDisplay);

            QFont dayFont = fitFontToHeight(boldFont, dayRect.height(), 1.2);
            painter.setFont(dayFont);
            painter.drawText(dayRect, Qt::AlignLeft | Qt::AlignBottom, dayText);
        } else {
            const double dayBlockWidth = qBound(24.0, contentRect.width() * 0.25, 72.0);
            QRectF dayRect(contentRect.left(), contentRect.top(), dayBlockWidth, contentRect.height());
            QRectF infoRect(dayRect.right() + padding, contentRect.top(), contentRect.width() - dayBlockWidth - padding, contentRect.height());

            QFont dayFont = fitFontToHeight(boldFont, dayRect.height(), 1.5);
            painter.setFont(dayFont);
            painter.drawText(dayRect, Qt::AlignLeft | Qt::AlignVCenter, dayText);

            QFont infoFont = baseFont;
            infoFont.setBold(false);
            painter.setFont(infoFont);
            QFontMetrics infoMetrics(infoFont);
            QRectF weekdayRect(infoRect.left(), infoRect.top(), infoRect.width(), infoRect.height() / 2);
            QRectF monthRect(infoRect.left(), infoRect.center().y(), infoRect.width(), infoRect.height() / 2);
            const QString weekdayDisplay = weekdayForWidth(date, infoMetrics, weekdayRect.width());
            const QString monthDisplay = monthYearForWidth(date, infoMetrics, monthRect.width());
            painter.drawText(weekdayRect, Qt::AlignLeft | Qt::AlignVCenter, weekdayDisplay);
            painter.drawText(monthRect, Qt::AlignLeft | Qt::AlignVCenter, monthDisplay);
        }

        if (isToday) {
            highlightLines[x] = QColor(0, 150, 0);
            highlightLines[x + m_dayWidth] = QColor(0, 150, 0);
        }
        if (date.day() == 1 && highlightLines.find(x) == highlightLines.end()) {
            highlightLines[x] = Qt::black;
        }
    }

    painter.setFont(originalFont);
    painter.setPen(palette().dark().color());

    const double leftBreak = qMax(0.0, m_timeAxisWidth - 5.0);
    for (int hour = 0; hour <= 24; ++hour) {
        const double y = bodyOriginY + hour * m_hourHeight;
        if (y < totalHeaderHeight - m_hourHeight || y > viewport()->height() + m_hourHeight) {
            continue;
        }
        painter.setPen(palette().mid().color());
        if (hour > 0) {
            painter.drawLine(QPointF(leftBreak, y), QPointF(m_timeAxisWidth, y));
        }

        painter.setPen(palette().windowText().color());
        QRectF labelRect(0, y - m_hourHeight / 2.0, leftBreak - 2.0, m_hourHeight);
        Qt::Alignment alignment = Qt::AlignRight | Qt::AlignVCenter;
        if (hour == 0) {
            alignment = Qt::AlignRight | Qt::AlignTop;
            labelRect.setTop(y);
            labelRect.setBottom(y + m_hourHeight);
        } else if (hour == 24) {
            alignment = Qt::AlignRight | Qt::AlignBottom;
            labelRect.setTop(y - m_hourHeight);
            labelRect.setBottom(y);
        }
        painter.drawText(labelRect,
                         alignment,
                         QStringLiteral("%1:00").arg(hour, 2, 10, QLatin1Char('0')));
    }

    painter.setPen(palette().dark().color());
    for (int day = 0; day < daySlots; ++day) {
        const double x = dayColumnLeft(day);
        painter.drawRect(QRectF(x, bodyOriginY, m_dayWidth, bodyHeight));
    }

    const double clipHeight = qMax(0.0, static_cast<double>(viewport()->height()) - totalHeaderHeight);
    const double clipWidth = qMax(0.0, static_cast<double>(viewport()->width()) - m_timeAxisWidth);
    painter.save();
    painter.setClipRect(QRectF(m_timeAxisWidth, totalHeaderHeight, clipWidth, clipHeight));

    painter.setPen(palette().mid().color());
    for (int hour = 0; hour <= 24; ++hour) {
        const double y = bodyOriginY + hour * m_hourHeight;
        if (y < totalHeaderHeight - m_hourHeight || y > viewport()->height() + m_hourHeight) {
            continue;
        }
        painter.drawLine(QPointF(m_timeAxisWidth, y), QPointF(totalWidth, y));
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF visibleRect(m_timeAxisWidth,
                             totalHeaderHeight - m_hourHeight,
                             qMax(0.0, static_cast<double>(viewport()->width()) - m_timeAxisWidth),
                             viewport()->height() - totalHeaderHeight + m_hourHeight * 2);
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

    auto paintSingleEvent = [&](const data::CalendarEvent &eventData) {
        const auto segments = segmentsForEvent(eventData);
        if (segments.empty()) {
            return;
        }

        const bool matchesFilter = eventMatchesFilter(eventData);
        QColor color = palette().highlight().color();
        QColor textColor = Qt::white;
        if (!matchesFilter) {
            color = palette().midlight().color();
            textColor = palette().mid().color();
        }
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
        QColor handleColor = color.lighter(130);
        handleColor.setAlpha(160);

        const bool hasAdjacentFollower = std::any_of(m_events.begin(), m_events.end(), [&](const data::CalendarEvent &other) {
            if (other.id == eventData.id) {
                return false;
            }
            if (other.start != eventData.end) {
                return false;
            }
            return other.start.date() == eventData.end.date();
        });

        for (const auto &segment : segments) {
            QRectF baseRect = adjustedRectForSegment(eventData, segment);
            QRectF rect = baseRect.translated(0, -yOffset);
            if (!rect.intersects(visibleRect)) {
                continue;
            }
            QPainterPath path = segmentPath(rect, segment.clipTop, segment.clipBottom);
            QPainterPath fillPath = path;
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawPath(fillPath);
            painter.setPen(QPen(color.darker(140), 1.2));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);

            painter.setPen(textColor);
            QString block;
            if (segment.clipTop) {
                block = tr("... %1").arg(eventData.title);
            } else if (!infoDrawn) {
                block = titleBlock;
            } else {
                block = eventData.title;
            }
            QRectF textRect = rect.adjusted(4, 2, -4, -4);
            QTextOption textOpt(Qt::AlignLeft | Qt::AlignTop);
            textOpt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            painter.drawText(textRect, block, textOpt);
            infoDrawn = true;

            bool showEndLabel = durationMinutes > 60;
            QString endLabel;
            if (segment.clipBottom) {
                endLabel = QStringLiteral("...");
            } else if (hasAdjacentFollower) {
                showEndLabel = false;
            } else {
                endLabel = segment.segmentEnd.time().toString(QStringLiteral("hh:mm"));
            }
            if (showEndLabel) {
                painter.drawText(QRectF(rect.left() + 4,
                                        rect.bottom() - 20,
                                        rect.width() - 8,
                                        18),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 endLabel);
            }
        }

        painter.setPen(Qt::NoPen);
        if (eventData.id == m_hoverTopHandleId) {
            QRectF translatedTop = adjustedRectForSegment(eventData, segments.front()).translated(0, -yOffset);
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
            QRectF translatedBottom = adjustedRectForSegment(eventData, segments.back()).translated(0, -yOffset);
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
    };

    const auto paintTodayLine = [&]() {
        const QDate todayLineDate = QDate::currentDate();
        if (todayLineDate < m_startDate || todayLineDate >= m_startDate.addDays(daySlots)) {
            return;
        }
        const int dayIndex = qBound(0,
                                    static_cast<int>(m_startDate.daysTo(todayLineDate)),
                                    qMax(0, daySlots - 1));
        const QTime now = QTime::currentTime();
        const double minutes = now.hour() * 60.0 + now.minute() + now.second() / 60.0;
        const double y = bodyOriginY + (minutes / 60.0) * m_hourHeight;
        if (y < totalHeaderHeight || y > bodyOriginY + bodyHeight) {
            return;
        }
        painter.setPen(QPen(Qt::red, 2));
        const double xStart = dayColumnLeft(dayIndex);
        painter.drawLine(QPointF(xStart, y), QPointF(xStart + m_dayWidth, y));
    };

    std::vector<const data::CalendarEvent *> baseEvents;
    std::vector<const data::CalendarEvent *> overlayEvents;
    const data::CalendarEvent *frontEvent = nullptr;
    const QUuid frontId = !m_selectedEvent.isNull() ? m_selectedEvent : m_hoveredEventId;
    for (const auto &event : m_events) {
        if (!frontId.isNull() && event.id == frontId) {
            frontEvent = &event;
            continue;
        }
        if (eventHasOverlay(event)) {
            overlayEvents.push_back(&event);
        } else {
            baseEvents.push_back(&event);
        }
    }
    auto sortEvents = [](std::vector<const data::CalendarEvent *> &events) {
        std::sort(events.begin(), events.end(), [](const data::CalendarEvent *lhs, const data::CalendarEvent *rhs) {
            if (lhs->start == rhs->start) {
                return lhs->end < rhs->end;
            }
            return lhs->start < rhs->start;
        });
    };
    sortEvents(baseEvents);
    sortEvents(overlayEvents);

    for (const auto *event : baseEvents) {
        paintSingleEvent(*event);
    }
    for (const auto *event : overlayEvents) {
        paintSingleEvent(*event);
    }
    if (frontEvent) {
        paintSingleEvent(*frontEvent);
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
                QFont original = painter.font();
                painter.setPen(Qt::white);
                QTextOption titleOpt(Qt::AlignLeft | Qt::AlignTop);
                titleOpt.setWrapMode(QTextOption::NoWrap);
                painter.drawText(QRectF(rect.left() + 4, rect.top() + 2, rect.width() - 8, rect.height() * 0.2),
                                 m_dropPreviewText,
                                 titleOpt);

                QFont startFont = original;
                startFont.setBold(true);
                startFont.setPointSizeF(startFont.pointSizeF() * 1.2);
                painter.setFont(startFont);
                painter.setPen(Qt::black);
                painter.drawText(QRectF(rect.left() + 4,
                                        rect.top() + rect.height() * 0.22,
                                        rect.width() - 8,
                                        rect.height() * 0.25),
                                 Qt::AlignLeft | Qt::AlignTop,
                                 segment.segmentStart.time().toString(QStringLiteral("hh:mm")));

                QFont dateFont = original;
                dateFont.setBold(true);
                dateFont.setPointSizeF(dateFont.pointSizeF() * 1.3);
                painter.setFont(dateFont);
                const QString dayText = QLocale().toString(segment.segmentStart.date(), QStringLiteral("dddd d."));
                painter.drawText(QRectF(rect.left() + 4,
                                        rect.top() + rect.height() * 0.5,
                                        rect.width() - 8,
                                        rect.height() * 0.35),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 dayText);
                painter.setFont(original);
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

    paintTodayLine();
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

bool CalendarView::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *helpEvent = static_cast<QHelpEvent *>(event);
        const QPoint pos = helpEvent->pos();
        const QPointF scenePos = QPointF(pos) + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
        if (const auto *eventData = eventAt(scenePos)) {
            const QString tooltip = eventTooltipText(*eventData);
            if (!tooltip.isEmpty()) {
                QToolTip::showText(helpEvent->globalPos(), tooltip, viewport());
                return true;
            }
        }
        const double monthHeight = monthBandHeight();
        const double totalHeight = totalHeaderHeight();
        if (m_dayWidth > 0.0 && pos.x() >= m_timeAxisWidth) {
            const double dayPosition = mapToDayPosition(pos.x());
            if (dayPosition >= 0.0) {
                const int slotCount = daySlotCount();
                const int dayIndex = qBound(0, static_cast<int>(dayPosition), qMax(0, slotCount - 1));
            if (pos.y() >= monthHeight && pos.y() <= totalHeight) {
                const QDate date = m_startDate.addDays(dayIndex);
                const QString tooltip = QLocale().toString(date, QLocale::LongFormat);
                QToolTip::showText(helpEvent->globalPos(), tooltip, viewport());
                return true;
            }
            if (showMonthBand() && pos.y() < monthHeight) {
                const QDate date = m_startDate.addDays(dayIndex);
                const QString tooltip = QLocale().toString(date, QStringLiteral("MMMM yyyy"));
                QToolTip::showText(helpEvent->globalPos(), tooltip, viewport());
                return true;
            }
            }
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    return QAbstractScrollArea::viewportEvent(event);
}

void CalendarView::mousePressEvent(QMouseEvent *event)
{
    ensureLayoutCache();
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
        storePointerPosition(event->pos());
        m_pressPos = event->pos();
        resetDragCandidate();
        cancelNewEventDrag();
        const auto orderedEvents = eventsInHitOrder();
        for (const auto *evPtr : orderedEvents) {
            const auto &ev = *evPtr;
            const auto segments = segmentsForEvent(ev);
            if (segments.empty()) {
                continue;
            }
            const QRectF topRect = adjustedRectForSegment(ev, segments.front());
            const bool overTopHandle = scenePos.x() >= topRect.left()
                && scenePos.x() <= topRect.right()
                && m_hoverTopHandleId == ev.id;
            if (overTopHandle) {
                auto topArea = handleArea(ev, true);
                if (scenePos.y() >= topArea.first && scenePos.y() <= topArea.second) {
                    beginResize(ev, true);
                    m_pendingResizeEvent = ev.id;
                    m_resizeAdjustStart = true;
                    m_newEventDragPending = false;
                    m_newEventAnchorTime = QDateTime();
                    event->accept();
                    return;
                }
            }

            const QRectF bottomRect = adjustedRectForSegment(ev, segments.back());
            const bool overBottomHandle = scenePos.x() >= bottomRect.left()
                && scenePos.x() <= bottomRect.right()
                && m_hoverBottomHandleId == ev.id;
            if (overBottomHandle) {
                auto bottomArea = handleArea(ev, false);
                if (scenePos.y() >= bottomArea.first && scenePos.y() <= bottomArea.second) {
                    beginResize(ev, false);
                    m_pendingResizeEvent = ev.id;
                    m_resizeAdjustStart = false;
                    m_newEventDragPending = false;
                    m_newEventAnchorTime = QDateTime();
                    event->accept();
                    return;
                }
            }

            bool contains = false;
            for (const auto &segment : segments) {
                QRectF adjusted = adjustedRectForSegment(ev, segment);
                if (adjusted.contains(scenePos)) {
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
    storePointerPosition(event->pos());
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
    if ((event->buttons() & Qt::LeftButton) && !m_pendingResizeEvent.isNull()) {
        auto it = std::find_if(m_events.begin(), m_events.end(), [this](const data::CalendarEvent &ev) {
            return ev.id == m_pendingResizeEvent;
        });
        if (it != m_events.end()) {
            beginResize(*it, m_resizeAdjustStart);
            const QPointF scene = QPointF(event->pos())
                + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
            updateResize(scene);
        }
        m_pendingResizeEvent = QUuid();
        event->accept();
        return;
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
    storePointerPosition(event->pos());
    if (m_dragMode != DragMode::None && event->button() == Qt::LeftButton) {
        endResize();
        clearPointerPosition();
        event->accept();
        return;
    }
    if (m_internalDragActive && event->button() == Qt::LeftButton) {
        const QPointF scenePos = QPointF(event->pos())
            + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
        finalizeInternalEventDrag(scenePos);
        clearPointerPosition();
        event->accept();
        return;
    }
    if (m_newEventDragActive && event->button() == Qt::LeftButton) {
        const QPointF scenePos = QPointF(event->pos())
            + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
        updateNewEventDrag(scenePos);
        finalizeNewEventDrag();
        clearPointerPosition();
        event->accept();
        return;
    }
    if (m_newEventDragPending && event->button() == Qt::LeftButton) {
        cancelNewEventDrag();
    }
    resetDragCandidate();
    clearPointerPosition();
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void CalendarView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mouseDoubleClickEvent(event);
        return;
    }
    const QPointF scenePos = QPointF(event->pos())
        + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    if (const auto *eventData = eventAt(scenePos)) {
        emit inlineEditRequested(*eventData);
        event->accept();
        return;
    }
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}

void CalendarView::dragEnterEvent(QDragEnterEvent *event)
{
    storePointerPosition(event->pos());
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
    storePointerPosition(event->pos());
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
        dateTime = dateTime.addSecs(-placementOffsetMinutes(durationMinutes) * 60);
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
    storePointerPosition(event->pos());
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
        const int durationMinutes = entry->durationMinutes > 0 ? entry->durationMinutes : 60;
        dateTime = dateTime.addSecs(-placementOffsetMinutes(durationMinutes) * 60);
        const bool copy = event->keyboardModifiers().testFlag(Qt::ControlModifier);
        emit todoDropped(entry->id, dateTime, copy);
        temporarilyDisableNewEventCreation();
        event->setDropAction(copy ? Qt::CopyAction : Qt::MoveAction);
        event->accept();
        clearPointerPosition();
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
        temporarilyDisableNewEventCreation();
        event->setDropAction(copy ? Qt::CopyAction : Qt::MoveAction);
        event->accept();
        clearPointerPosition();
        return;
    }

    event->ignore();
    clearPointerPosition();
}

void CalendarView::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_autoScrollTimer.invalidate();
    m_dragInteractionActive = false;
    clearPointerPosition();
    Q_UNUSED(event);
    clearDropPreview();
}

void CalendarView::leaveEvent(QEvent *event)
{
    QAbstractScrollArea::leaveEvent(event);
    if (!m_hoveredEventId.isNull()) {
        m_hoveredEventId = QUuid();
        viewport()->update();
    }
    clearPointerPosition();
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
    const int slotCount = daySlotCount();
    for (int day = 0; day < slotCount; ++day) {
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
        const double y = totalHeaderHeight() + (startMinutes / 60.0) * m_hourHeight;
        const double durationMinutes = qMax(0.0, endMinutes - startMinutes);
        const double height = qMax((durationMinutes / 60.0) * m_hourHeight, 20.0);
        const double x = dayColumnLeft(day) + 6.0;
        const double width = qMax(0.0, m_dayWidth - 12.0);

        EventSegment segment;
        segment.rect = QRectF(x, y, width, height);
        segment.segmentStart = segmentStart;
        segment.segmentEnd = segmentEnd;
        segment.clipTop = segmentStart > event.start;
        segment.clipBottom = segmentEnd < event.end;
        segment.dayIndex = day;
        segments.push_back(segment);
    }
    return segments;
}

void CalendarView::invalidateLayout()
{
    m_layoutDirty = true;
}

void CalendarView::ensureLayoutCache() const
{
    if (!m_layoutDirty) {
        return;
    }
    m_layoutCache.clear();
    const int slotCount = daySlotCount();
    if (slotCount <= 0) {
        m_layoutDirty = false;
        return;
    }

    struct DayEntry
    {
        const data::CalendarEvent *event = nullptr;
        int dayIndex = -1;
        double startMinutes = 0.0;
        double endMinutes = 0.0;
        double width = 1.0;
        double offset = 0.0;
        LayoutInfo::Anchor anchor = LayoutInfo::Anchor::Left;
        bool fromSplit = false;
        bool isContained = false;
    };

    std::vector<std::vector<DayEntry>> perDay(static_cast<std::size_t>(slotCount));
    for (const auto &event : m_events) {
        const auto segments = segmentsForEvent(event);
        for (const auto &segment : segments) {
            if (segment.dayIndex < 0 || segment.dayIndex >= slotCount) {
                continue;
            }
            DayEntry entry;
            entry.event = &event;
            entry.dayIndex = segment.dayIndex;
            const QTime startTime = segment.segmentStart.time();
            const QTime endTime = segment.segmentEnd.time();
            entry.startMinutes = startTime.hour() * 60.0 + startTime.minute() + startTime.second() / 60.0;
            entry.endMinutes = endTime.hour() * 60.0 + endTime.minute() + endTime.second() / 60.0;
            if (segment.segmentEnd.date() > segment.segmentStart.date()
                && segment.segmentEnd.time() == QTime(0, 0)) {
                entry.endMinutes = 24.0 * 60.0;
            }
            perDay[static_cast<std::size_t>(segment.dayIndex)].push_back(entry);
        }
    }

    constexpr double containWidth = 0.58;
    constexpr double containerWidth = 0.88;
    constexpr double overlapWidth = 0.72;
    constexpr double epsilon = 0.01;

    for (auto &entries : perDay) {
        if (entries.empty()) {
            continue;
        }
        std::map<std::pair<int, int>, std::vector<int>> identical;
        for (int idx = 0; idx < entries.size(); ++idx) {
            const int startKey = qRound(entries[idx].startMinutes * 10.0);
            const int endKey = qRound(entries[idx].endMinutes * 10.0);
            identical[{ startKey, endKey }].push_back(idx);
        }
        for (auto &[_, indices] : identical) {
            if (indices.size() <= 1) {
                continue;
            }
            const double width = qBound(0.25,
                                        1.0 / static_cast<double>(indices.size()),
                                        0.5);
            for (int position = 0; position < indices.size(); ++position) {
                auto &entry = entries[indices[position]];
                entry.width = width;
                entry.offset = width * position;
                if (position == 0) {
                    entry.anchor = LayoutInfo::Anchor::Left;
                } else if (position == indices.size() - 1) {
                    entry.anchor = LayoutInfo::Anchor::Right;
                } else {
                    entry.anchor = LayoutInfo::Anchor::Center;
                }
                entry.fromSplit = true;
            }
        }

        for (int i = 0; i < entries.size(); ++i) {
            for (int j = i + 1; j < entries.size(); ++j) {
                auto &first = entries[i];
                auto &second = entries[j];
                bool sameRange = qAbs(first.startMinutes - second.startMinutes) < epsilon
                    && qAbs(first.endMinutes - second.endMinutes) < epsilon;
                if (sameRange && first.fromSplit && second.fromSplit) {
                    continue;
                }
                bool firstContainsSecond = first.startMinutes <= second.startMinutes + epsilon
                    && first.endMinutes >= second.endMinutes - epsilon;
                bool secondContainsFirst = second.startMinutes <= first.startMinutes + epsilon
                    && second.endMinutes >= first.endMinutes - epsilon;

                if (firstContainsSecond && !secondContainsFirst) {
                    if (!first.fromSplit) {
                        first.width = qMin(first.width, containerWidth);
                        first.offset = 0.0;
                        first.anchor = LayoutInfo::Anchor::Left;
                    }
                    second.width = qMin(second.width, containWidth);
                    second.offset = 1.0 - second.width;
                    second.anchor = LayoutInfo::Anchor::Right;
                    second.isContained = true;
                    continue;
                }
                if (secondContainsFirst && !firstContainsSecond) {
                    if (!second.fromSplit) {
                        second.width = qMin(second.width, containerWidth);
                        second.offset = 0.0;
                        second.anchor = LayoutInfo::Anchor::Left;
                    }
                    first.width = qMin(first.width, containWidth);
                    first.offset = 1.0 - first.width;
                    first.anchor = LayoutInfo::Anchor::Right;
                    first.isContained = true;
                    continue;
                }
                const double overlapStart = qMax(first.startMinutes, second.startMinutes);
                const double overlapEnd = qMin(first.endMinutes, second.endMinutes);
                if (overlapEnd - overlapStart > epsilon) {
                    DayEntry *left = &first;
                    DayEntry *right = &second;
                    if (second.startMinutes < first.startMinutes
                        || (qAbs(second.startMinutes - first.startMinutes) < epsilon
                            && second.endMinutes < first.endMinutes)) {
                        left = &second;
                        right = &first;
                    }
                    if (!left->fromSplit && !left->isContained) {
                        left->width = qMin(left->width, overlapWidth);
                        left->offset = 0.0;
                        left->anchor = LayoutInfo::Anchor::Left;
                    }
                    if (!right->fromSplit && !right->isContained) {
                        right->width = qMin(right->width, overlapWidth);
                        right->offset = 1.0 - right->width;
                        right->anchor = LayoutInfo::Anchor::Right;
                    }
                }
            }
        }

        for (const auto &entry : entries) {
            LayoutInfo info;
            info.offsetFraction = entry.offset;
            info.widthFraction = entry.width;
            info.anchor = entry.anchor;
            if (entry.isContained) {
                info.zPriority = 1;
            }
            m_layoutCache[{ entry.event->id, entry.dayIndex }] = info;
        }
    }

    m_layoutDirty = false;
}

CalendarView::LayoutInfo CalendarView::layoutInfoFor(const QUuid &eventId, int dayIndex) const
{
    ensureLayoutCache();
    auto key = std::make_pair(eventId, dayIndex);
    const auto it = m_layoutCache.find(key);
    if (it != m_layoutCache.end()) {
        return it->second;
    }
    return LayoutInfo();
}

QRectF CalendarView::adjustedRectForSegment(const data::CalendarEvent &event, const EventSegment &segment) const
{
    LayoutInfo info = layoutInfoFor(event.id, segment.dayIndex);
    QRectF rect = segment.rect;
    const double availableWidth = rect.width();
    double width = availableWidth * info.widthFraction;
    width = qBound(0.0, width, availableWidth);
    double x = rect.left() + info.offsetFraction * availableWidth;
    const QUuid frontId = !m_hoveredEventId.isNull() ? m_hoveredEventId : m_selectedEvent;
    if (!frontId.isNull() && frontId == event.id && eventHasOverlap(event)) {
        double hoverWidth = availableWidth * 0.85;
        hoverWidth = qBound(0.0, hoverWidth, availableWidth);
        switch (info.anchor) {
        case LayoutInfo::Anchor::Left:
            x = rect.left();
            break;
        case LayoutInfo::Anchor::Right:
            x = rect.right() - hoverWidth;
            break;
        case LayoutInfo::Anchor::Center:
            x = rect.center().x() - hoverWidth / 2.0;
            break;
        }
        width = hoverWidth;
    }
    rect.setLeft(x);
    rect.setWidth(width);
    return rect;
}

bool CalendarView::eventHasOverlap(const data::CalendarEvent &event) const
{
    ensureLayoutCache();
    for (const auto &[key, info] : m_layoutCache) {
        if (key.first == event.id) {
            if (!qFuzzyCompare(1.0 + info.widthFraction, 1.0 + 1.0) || info.offsetFraction > 0.0) {
                return true;
            }
        }
    }
    return false;
}

bool CalendarView::eventHasOverlay(const data::CalendarEvent &event) const
{
    ensureLayoutCache();
    for (const auto &[key, info] : m_layoutCache) {
        if (key.first == event.id && info.zPriority > 0) {
            return true;
        }
    }
    return false;
}

std::vector<const data::CalendarEvent *> CalendarView::eventsInHitOrder() const
{
    std::vector<const data::CalendarEvent *> overlays;
    std::vector<const data::CalendarEvent *> base;
    overlays.reserve(m_events.size());
    base.reserve(m_events.size());
    const QUuid frontId = !m_hoveredEventId.isNull() ? m_hoveredEventId : m_selectedEvent;
    const data::CalendarEvent *frontEvent = nullptr;
    for (const auto &event : m_events) {
        if (!frontId.isNull() && event.id == frontId) {
            frontEvent = &event;
            continue;
        }
        if (eventHasOverlay(event)) {
            overlays.push_back(&event);
        } else {
            base.push_back(&event);
        }
    }
    auto sortByTime = [](const data::CalendarEvent *lhs, const data::CalendarEvent *rhs) {
        if (lhs->start == rhs->start) {
            return lhs->end < rhs->end;
        }
        return lhs->start < rhs->start;
    };
    std::sort(overlays.begin(), overlays.end(), sortByTime);
    std::sort(base.begin(), base.end(), sortByTime);
    std::vector<const data::CalendarEvent *> ordered;
    ordered.reserve(overlays.size() + base.size() + 1);
    if (frontEvent) {
        ordered.push_back(frontEvent);
    }
    ordered.insert(ordered.end(), overlays.begin(), overlays.end());
    ordered.insert(ordered.end(), base.begin(), base.end());
    return ordered;
}

void CalendarView::updateScrollBars()
{
    const double bodyHeight = m_hourHeight * 24.0;
    const double header = totalHeaderHeight();
    const int pageStep = qMax(1, static_cast<int>(viewport()->height() - header));
    horizontalScrollBar()->setRange(0, 0);
    horizontalScrollBar()->setPageStep(viewport()->width());
    horizontalScrollBar()->setValue(0);

    verticalScrollBar()->setRange(0, qMax(0, static_cast<int>(bodyHeight - pageStep)));
    verticalScrollBar()->setPageStep(pageStep);
}

void CalendarView::selectEventAt(const QPoint &pos)
{
    ensureLayoutCache();
    const QPointF scenePos = QPointF(pos) + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const auto orderedEvents = eventsInHitOrder();
    for (const auto *eventData : orderedEvents) {
        const auto segments = segmentsForEvent(*eventData);
        for (const auto &segment : segments) {
            QRectF adjusted = adjustedRectForSegment(*eventData, segment);
            if (adjusted.contains(scenePos)) {
                m_selectedEvent = eventData->id;
                viewport()->update();
                emit eventActivated(*eventData);
                emit eventSelected(*eventData);
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
    ensureLayoutCache();
    if (m_dayWidth <= 0.0) {
        return;
    }
    const QPointF scenePos = QPointF(pos) + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const double y = scenePos.y() - totalHeaderHeight();
    if (y >= 0) {
        const double hours = y / m_hourHeight;
        const int hour = static_cast<int>(hours);
        const int minute = static_cast<int>((hours - hour) * 60);
        const double dayPosition = mapToDayPosition(scenePos.x());
        const int dayIndex = static_cast<int>(dayPosition);
        const int slotCount = daySlotCount();
        if (dayPosition >= 0.0 && dayIndex >= 0 && dayIndex < slotCount) {
            QDateTime dt(m_startDate.addDays(dayIndex), QTime(hour, minute));
            emit hoveredDateTime(dt);
        }
    }

    QUuid newTop;
    QUuid newBottom;
    const bool selectionActive = !m_selectedEvent.isNull();
    const auto orderedEvents = eventsInHitOrder();
    for (const auto *eventData : orderedEvents) {
        const auto segments = segmentsForEvent(*eventData);
        if (segments.empty()) {
            continue;
        }
        if (selectionActive && eventData->id != m_selectedEvent) {
            continue;
        }
        const QRectF topRect = adjustedRectForSegment(*eventData, segments.front());
        if (scenePos.x() >= topRect.left() && scenePos.x() <= topRect.right()) {
            auto topArea = handleArea(*eventData, true);
            if (scenePos.y() >= topArea.first && scenePos.y() <= topArea.second) {
                newTop = eventData->id;
            }
        }

        const QRectF bottomRect = adjustedRectForSegment(*eventData, segments.back());
        if (scenePos.x() >= bottomRect.left() && scenePos.x() <= bottomRect.right()) {
            auto bottomArea = handleArea(*eventData, false);
            if (scenePos.y() >= bottomArea.first && scenePos.y() <= bottomArea.second) {
                newBottom = eventData->id;
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

    const auto *hoveredEvent = eventAt(scenePos);
    const QUuid hoveredId = hoveredEvent ? hoveredEvent->id : QUuid();
    if (hoveredId != m_hoveredEventId) {
        m_hoveredEventId = hoveredId;
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
    m_pendingResizeEvent = QUuid();
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
    const double y = scenePos.y() - totalHeaderHeight();
    if (y < 0) {
        return std::nullopt;
    }
    const double dayPosition = mapToDayPosition(scenePos.x());
    if (dayPosition < 0.0) {
        return std::nullopt;
    }
    const int dayIndex = static_cast<int>(dayPosition);
    const int slotCount = daySlotCount();
    if (dayIndex < 0 || dayIndex >= slotCount) {
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
    const_cast<CalendarView *>(this)->ensureLayoutCache();
    const auto orderedEvents = eventsInHitOrder();
    for (const auto *eventData : orderedEvents) {
        const auto segments = segmentsForEvent(*eventData);
        for (const auto &segment : segments) {
            QRectF adjusted = const_cast<CalendarView *>(this)->adjustedRectForSegment(*eventData, segment);
            if (adjusted.contains(scenePos)) {
                return eventData;
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
        temporarilyDisableNewEventCreation();
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
    const_cast<CalendarView *>(this)->ensureLayoutCache();
    const auto segments = segmentsForEvent(event);
    if (segments.empty()) {
        return { 0.0, 0.0 };
    }
    const QRectF rect = const_cast<CalendarView *>(this)->adjustedRectForSegment(event, top ? segments.front() : segments.back());
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
        invalidateLayout();
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
        const double stepDays = horizontalWheelStepDays();
        if (stepDays > 0.0) {
            const double ticks = static_cast<double>(angle.x()) / 120.0;
            if (!qFuzzyIsNull(ticks)) {
                emit fractionalDayScrollRequested(-ticks * stepDays);
                handled = true;
            }
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

double CalendarView::horizontalWheelStepDays() const
{
    if (m_dayWidth <= 0.0) {
        return 0.0;
    }
    const double visibleWidth = qMax(0.0, static_cast<double>(viewport()->width()) - m_timeAxisWidth);
    if (visibleWidth <= 0.0) {
        return 0.0;
    }
    const double desiredDays = (visibleWidth * 0.05) / m_dayWidth;
    static constexpr std::array<double, 4> kSteps = { 1.0, 0.5, 1.0 / 3.0, 0.25 };
    double best = kSteps.front();
    double bestDiff = std::numeric_limits<double>::max();
    for (double step : kSteps) {
        const double diff = std::abs(step - desiredDays);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = step;
        }
    }
    return best;
}

double CalendarView::dayColumnLeft(int dayIndex) const
{
    return m_timeAxisWidth + (static_cast<double>(dayIndex) - m_dayOffset) * m_dayWidth;
}

double CalendarView::contentRightEdge() const
{
    return m_timeAxisWidth + (static_cast<double>(daySlotCount()) - m_dayOffset) * m_dayWidth;
}

double CalendarView::mapToDayPosition(double x) const
{
    if (m_dayWidth <= 0.0) {
        return -1.0;
    }
    return (x - m_timeAxisWidth) / m_dayWidth + m_dayOffset;
}

bool CalendarView::hasTrailingPartialDay() const
{
    constexpr double Epsilon = 1e-6;
    return m_dayOffset > Epsilon;
}

int CalendarView::daySlotCount() const
{
    return m_dayCount + (hasTrailingPartialDay() ? 1 : 0);
}

void CalendarView::scrollContentsBy(int dx, int dy)
{
    QAbstractScrollArea::scrollContentsBy(dx, dy);
    refreshActiveDragPreview();
}

void CalendarView::refreshActiveDragPreview()
{
    if (!m_lastPointerPosValid) {
        return;
    }
    const QPointF scenePos = QPointF(m_lastPointerPos)
        + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    if (m_dragMode != DragMode::None) {
        updateResize(scenePos);
        return;
    }
    if (m_internalDragActive) {
        updateInternalEventDrag(scenePos);
        return;
    }
    if (m_newEventDragActive) {
        updateNewEventDrag(scenePos);
        return;
    }
    if (m_dragInteractionActive && m_showDropPreview) {
        if (auto dateTimeOpt = dateTimeAtScene(scenePos)) {
            const int durationMinutes = qMax(1,
                                             static_cast<int>(m_dropPreviewEvent.start.secsTo(m_dropPreviewEvent.end)
                                                              / 60));
            updateDropPreview(snapDateTime(dateTimeOpt.value()), durationMinutes, m_dropPreviewText);
        }
    }
}

void CalendarView::storePointerPosition(const QPoint &pos)
{
    m_lastPointerPos = pos;
    m_lastPointerPosValid = true;
}

void CalendarView::clearPointerPosition()
{
    m_lastPointerPosValid = false;
}

QString CalendarView::eventTooltipText(const data::CalendarEvent &event) const
{
    QStringList lines;
    const QString title = event.title.trimmed().isEmpty() ? tr("(Ohne Titel)") : event.title.trimmed();
    lines << title;

    const QString description = event.description.trimmed();
    if (!description.isEmpty()) {
        lines << description;
    }

    const QString location = event.location.trimmed();
    if (!location.isEmpty()) {
        lines << tr("Ort: %1").arg(location);
    }

    const int durationMinutes = qMax(0, static_cast<int>(event.start.secsTo(event.end) / 60));
    const QString durationText = formatDurationMinutes(durationMinutes);
    if (!durationText.isEmpty()) {
        lines << tr("Dauer: %1").arg(durationText);
    }

    return lines.join(QStringLiteral("\n"));
}

QString CalendarView::formatDurationMinutes(int totalMinutes) const
{
    if (totalMinutes <= 0) {
        return QString();
    }
    const int hours = totalMinutes / 60;
    const int minutes = totalMinutes % 60;
    if (hours > 0 && minutes > 0) {
        return tr("%1h %2m").arg(hours).arg(minutes);
    }
    if (hours > 0) {
        return tr("%1h").arg(hours);
    }
    return tr("%1min").arg(minutes);
}

} // namespace ui
} // namespace calendar
