#pragma once

#include <QAbstractScrollArea>
#include <QDate>
#include <QVector>
#include <optional>
#include <vector>
#include <QString>
#include <QPair>

#include "calendar/data/Event.hpp"

namespace calendar {
namespace ui {

class CalendarView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit CalendarView(QWidget *parent = nullptr);

    void setDateRange(const QDate &start, int days);
    void setEvents(std::vector<data::CalendarEvent> events);
    void zoomTime(double factor);
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

public:
    void beginPlacementPreview(int durationMinutes, const QString &label, const QDateTime &initialStart);
    void updatePlacementPreview(const QDateTime &start);
    void cancelPlacementPreview();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;

private:
    QRectF eventRect(const data::CalendarEvent &event) const;
    void updateScrollBars();
    void selectEventAt(const QPoint &pos);
    void emitHoverAt(const QPoint &pos);
    int snapMinutes(double value) const;
    int snapIntervalMinutes(int value) const;
    void beginResize(const data::CalendarEvent &event, bool adjustStart);
    void updateResize(const QPointF &scenePos);
    void endResize();
    std::optional<QDateTime> dateTimeAtScene(const QPointF &scenePos) const;
    const data::CalendarEvent *eventAt(const QPointF &scenePos) const;
    void startEventDrag(const data::CalendarEvent &event, int pointerOffsetMinutes);
    void resetDragCandidate();
    void updateDropPreview(const QDateTime &start, int durationMinutes, const QString &label);
    void clearDropPreview();
    QPair<double, double> handleArea(const data::CalendarEvent &event, bool top) const;
    void recalculateDayWidth();

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
    QRectF m_dropPreviewRect;
    QString m_dropPreviewText;
    bool m_externalPlacementMode = false;
    int m_externalPlacementDuration = 0;
    QString m_externalPlacementLabel;
    QUuid m_hoverTopHandleId;
    QUuid m_hoverBottomHandleId;
    double m_horizontalScrollRemainder = 0.0;
};

} // namespace ui
} // namespace calendar
