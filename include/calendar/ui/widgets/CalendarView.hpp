#pragma once

#include <QAbstractScrollArea>
#include <QDate>
#include <QDateTime>
#include <QVector>
#include <optional>
#include <vector>
#include <QString>
#include <QPair>
#include <QElapsedTimer>
#include <map>

#include "calendar/data/Event.hpp"
#include "calendar/data/Todo.hpp"

namespace calendar {
namespace ui {

class CalendarView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit CalendarView(QWidget *parent = nullptr);
    ~CalendarView() override;

    void setDateRange(const QDate &start, int days);
    void setDayOffset(double offsetDays);
    void setEvents(std::vector<data::CalendarEvent> events);
    void zoomTime(double factor);
    double hourHeight() const { return m_hourHeight; }
    void setHourHeight(double height);
    int verticalScrollValue() const;
    void setVerticalScrollValue(int value);
    void setEventSearchFilter(const QString &text);
signals:
    void dayZoomRequested(bool zoomIn);
    void dayScrollRequested(int dayDelta);
    void fractionalDayScrollRequested(double dayDelta);

    void eventActivated(const data::CalendarEvent &event);
    void inlineEditRequested(const data::CalendarEvent &event);
    void hoveredDateTime(const QDateTime &dateTime);
    void eventSelected(const data::CalendarEvent &event);
    void eventResizeRequested(const QUuid &id, const QDateTime &newStart, const QDateTime &newEnd);
    void selectionCleared();
    void todoDropped(const QUuid &todoId, const QDateTime &start);
    void eventDropRequested(const QUuid &eventId, const QDateTime &start, bool copy);
    void externalPlacementConfirmed(const QDateTime &start);
    void eventDroppedToTodo(const data::CalendarEvent &event, data::TodoStatus status);
    void eventCreationRequested(const QDateTime &start, const QDateTime &end);
    void todoHoverPreviewRequested(data::TodoStatus status, const data::CalendarEvent &event);
    void todoHoverPreviewCleared();

public:
    void beginPlacementPreview(int durationMinutes, const QString &label, const QDateTime &initialStart);
    void updatePlacementPreview(const QDateTime &start);
    void cancelPlacementPreview();
    void clearExternalSelection();
    void clearGhostPreview();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool viewportEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateScrollBars();
    void selectEventAt(const QPoint &pos);
    void emitHoverAt(const QPoint &pos);
    int snapMinutes(double value) const;
    QDateTime snapDateTime(const QDateTime &value) const;
    int snapIntervalMinutes(int value) const;
    void beginResize(const data::CalendarEvent &event, bool adjustStart);
    void updateResize(const QPointF &scenePos);
    void endResize();
    std::optional<QDateTime> dateTimeAtScene(const QPointF &scenePos) const;
    const data::CalendarEvent *eventAt(const QPointF &scenePos) const;
    void resetDragCandidate();
    void updateDropPreview(const QDateTime &start, int durationMinutes, const QString &label);
    void clearDropPreview();
    QPair<double, double> handleArea(const data::CalendarEvent &event, bool top) const;
    void recalculateDayWidth();
    void maybeAutoScrollHorizontally(const QPoint &pos);
    bool handleWheelInteraction(QWheelEvent *event);
    double horizontalWheelStepDays() const;
    double dayColumnLeft(int dayIndex) const;
    double contentRightEdge() const;
    double mapToDayPosition(double x) const;
    int daySlotCount() const;
    bool hasTrailingPartialDay() const;
    void beginInternalEventDrag(const data::CalendarEvent &event, int pointerOffsetMinutes);
    void updateInternalEventDrag(const QPointF &scenePos);
    void finalizeInternalEventDrag(const QPointF &scenePos);
    void cancelInternalEventDrag();
    std::optional<data::TodoStatus> todoStatusUnderCursor(const QPoint &globalPos) const;
    void updateTodoHoverFeedback();
    void clearTodoHoverFeedback();
    struct EventSegment {
        QRectF rect;
        QDateTime segmentStart;
        QDateTime segmentEnd;
        bool clipTop = false;
        bool clipBottom = false;
        int dayIndex = -1;
    };
    std::vector<EventSegment> segmentsForEvent(const data::CalendarEvent &event) const;
    void startNewEventDrag();
    void updateNewEventDrag(const QPointF &scenePos);
    void finalizeNewEventDrag();
    void cancelNewEventDrag();
    void temporarilyDisableNewEventCreation();
    bool showMonthBand() const;
    double monthBandHeight() const;
    double totalHeaderHeight() const;
    bool eventMatchesFilter(const data::CalendarEvent &event) const;
    QString eventTooltipText(const data::CalendarEvent &event) const;
    QString formatDurationMinutes(int totalMinutes) const;
    void invalidateLayout();
    void ensureLayoutCache() const;
    struct LayoutInfo {
        double offsetFraction = 0.0;
        double widthFraction = 1.0;
        enum class Anchor
        {
            Left,
            Right,
            Center
        };
        Anchor anchor = Anchor::Left;
        int zPriority = 0;
    };
    LayoutInfo layoutInfoFor(const QUuid &eventId, int dayIndex) const;
    QRectF adjustedRectForSegment(const data::CalendarEvent &event, const EventSegment &segment) const;
    bool eventHasOverlap(const data::CalendarEvent &event) const;
    bool eventHasOverlay(const data::CalendarEvent &event) const;
    std::vector<const data::CalendarEvent *> eventsInHitOrder() const;

    QDate m_startDate;
    int m_dayCount = 5;
    double m_hourHeight = 60.0;
    double m_dayWidth = 200.0;
    double m_dayOffset = 0.0;
    double m_headerHeight = 40.0;
    double m_timeAxisWidth = 70.0;
    std::vector<data::CalendarEvent> m_events;
    QUuid m_selectedEvent;
    QUuid m_pendingResizeEvent;
    bool m_resizeAdjustStart = false;
    enum class DragMode
    {
        None,
        ResizeStart,
        ResizeEnd
    };
    DragMode m_dragMode = DragMode::None;
    data::CalendarEvent m_dragEvent;
    QUuid m_dragCandidateId;
    QPoint m_pressPos;
    bool m_draggingEvent = false;
    int m_dragPointerOffsetMinutes = 0;
    bool m_showDropPreview = false;
    data::CalendarEvent m_dropPreviewEvent;
    QString m_dropPreviewText;
    bool m_externalPlacementMode = false;
    int m_externalPlacementDuration = 0;
    QString m_externalPlacementLabel;
    QUuid m_hoverTopHandleId;
    QUuid m_hoverBottomHandleId;
    bool m_dragInteractionActive = false;
    bool m_forwardingKeyEvent = false;
    QElapsedTimer m_autoScrollTimer;
    bool m_internalDragActive = false;
    data::CalendarEvent m_internalDragSource;
    int m_internalDragOffsetMinutes = 0;
    int m_internalDragDurationMinutes = 0;
    bool m_newEventDragPending = false;
    bool m_newEventDragActive = false;
    QDateTime m_newEventAnchorTime;
    QDateTime m_newEventStart;
    QDateTime m_newEventEnd;
    bool m_allowNewEventCreation = true;
    std::optional<data::TodoStatus> m_currentTodoHoverStatus;
    QString m_eventSearchFilter;
    QUuid m_hoveredEventId;
    mutable bool m_layoutDirty = true;
    mutable std::map<std::pair<QUuid, int>, LayoutInfo> m_layoutCache;
};

} // namespace ui
} // namespace calendar
