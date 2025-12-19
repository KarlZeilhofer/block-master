#pragma once

#include <QDialog>

#include "calendar/data/Event.hpp"

class QLineEdit;
class QDateTimeEdit;
class QPlainTextEdit;
class QComboBox;
class QSpinBox;

namespace calendar {
namespace ui {

class EventDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EventDetailDialog(QWidget *parent = nullptr);

    void setEvent(const data::CalendarEvent &event);
    data::CalendarEvent event() const;

private:
    data::CalendarEvent m_event;
    QLineEdit *m_titleEdit = nullptr;
    QLineEdit *m_locationEdit = nullptr;
    QPlainTextEdit *m_descriptionEdit = nullptr;
    QDateTimeEdit *m_startEdit = nullptr;
    QDateTimeEdit *m_endEdit = nullptr;
    QComboBox *m_recurrenceCombo = nullptr;
    QSpinBox *m_reminderSpin = nullptr;
    QComboBox *m_categoryCombo = nullptr;
};

} // namespace ui
} // namespace calendar
