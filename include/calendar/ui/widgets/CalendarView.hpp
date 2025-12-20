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

    void eventActivated(const data::CalendarEvent &event);
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
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
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

    QDate m_startDate;
    int m_dayCount = 5;
    double m_hourHeight = 60.0;
    double m_dayWidth = 200.0;
    double m_headerHeight = 40.0;
    double m_timeAxisWidth = 70.0;
    std::vector<data::CalendarEvent> m_events;
    QUuid m_selectedEvent;
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
    double m_horizontalScrollRemainder = 0.0;
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
};

} // namespace ui
} // namespace calendar
