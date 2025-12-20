#pragma once

#include <QWidget>
#include <QShortcut>

#include "calendar/data/Event.hpp"
#include "calendar/data/Todo.hpp"

class QLineEdit;
class QDateTimeEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;

namespace calendar {
namespace ui {

class EventInlineEditor : public QWidget
{
    Q_OBJECT

public:
    explicit EventInlineEditor(QWidget *parent = nullptr);

    void setEvent(const data::CalendarEvent &event);
    void setTodo(const data::TodoItem &todo);
    void focusTitle(bool selectAll = false);
    void clearEditor();
    bool isTodoMode() const { return m_isTodo; }

signals:
    void saveRequested(const data::CalendarEvent &event);
    void saveTodoRequested(const data::TodoItem &todo);
    void cancelRequested();

private slots:
    void handleSave();
    void handleCancel();

private:
    data::CalendarEvent m_event;
    data::TodoItem m_todo;
    QLineEdit *m_titleEdit = nullptr;
    QLineEdit *m_locationEdit = nullptr;
    QDateTimeEdit *m_startEdit = nullptr;
    QDateTimeEdit *m_endEdit = nullptr;
    QPlainTextEdit *m_descriptionEdit = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QShortcut *m_saveShortcut = nullptr;
    QShortcut *m_escapeShortcut = nullptr;
    bool m_isTodo = false;
    QLabel *m_startLabel = nullptr;
    QLabel *m_endLabel = nullptr;
};

} // namespace ui
} // namespace calendar
