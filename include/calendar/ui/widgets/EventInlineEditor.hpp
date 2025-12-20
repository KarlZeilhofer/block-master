#pragma once

#include <QWidget>
#include <QShortcut>

#include "calendar/data/Event.hpp"

class QLineEdit;
class QDateTimeEdit;
class QPlainTextEdit;
class QPushButton;

namespace calendar {
namespace ui {

class EventInlineEditor : public QWidget
{
    Q_OBJECT

public:
    explicit EventInlineEditor(QWidget *parent = nullptr);

    void setEvent(const data::CalendarEvent &event);
    void clearEditor();

signals:
    void saveRequested(const data::CalendarEvent &event);
    void cancelRequested();

private slots:
    void handleSave();
    void handleCancel();

private:
    data::CalendarEvent m_event;
    QLineEdit *m_titleEdit = nullptr;
    QLineEdit *m_locationEdit = nullptr;
    QDateTimeEdit *m_startEdit = nullptr;
    QDateTimeEdit *m_endEdit = nullptr;
    QPlainTextEdit *m_descriptionEdit = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QShortcut *m_saveShortcut = nullptr;
    QShortcut *m_escapeShortcut = nullptr;
};

} // namespace ui
} // namespace calendar
