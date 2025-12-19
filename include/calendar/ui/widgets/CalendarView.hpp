#pragma once

#include <QAbstractScrollArea>
#include <QDate>
#include <QVector>
#include <vector>

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
    void zoomDays(double factor);

signals:
    void eventActivated(const data::CalendarEvent &event);
    void hoveredDateTime(const QDateTime &dateTime);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QRectF eventRect(const data::CalendarEvent &event) const;
    void updateScrollBars();
    void selectEventAt(const QPoint &pos);
    void emitHoverAt(const QPoint &pos);

    QDate m_startDate;
    int m_dayCount = 5;
    double m_hourHeight = 60.0;
    double m_dayWidth = 200.0;
    double m_headerHeight = 40.0;
    double m_timeAxisWidth = 70.0;
    std::vector<data::CalendarEvent> m_events;
    QUuid m_selectedEvent;
};

} // namespace ui
} // namespace calendar
