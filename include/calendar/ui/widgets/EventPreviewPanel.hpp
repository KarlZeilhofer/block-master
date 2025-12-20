#pragma once

#include <QWidget>

#include "calendar/data/Event.hpp"

class QLabel;

namespace calendar {
namespace ui {

class EventPreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EventPreviewPanel(QWidget *parent = nullptr);

    void setEvent(const data::CalendarEvent &event);
    void clearEvent();

private:
    QLabel *m_titleLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_locationLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QLabel *m_descriptionLabel = nullptr;
};

} // namespace ui
} // namespace calendar
