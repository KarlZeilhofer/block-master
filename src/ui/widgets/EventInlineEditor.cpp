#include "calendar/ui/widgets/EventInlineEditor.hpp"

#include <QDateTimeEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QShortcut>
#include <QKeyEvent>

namespace calendar {
namespace ui {

EventInlineEditor::EventInlineEditor(QWidget *parent)
    : QWidget(parent)
{
    setVisible(false);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(6);

    m_titleEdit = new QLineEdit(this);
    formLayout->addRow(tr("Titel"), m_titleEdit);
    connect(m_titleEdit, &QLineEdit::returnPressed, this, &EventInlineEditor::handleSave);

    m_locationEdit = new QLineEdit(this);
    formLayout->addRow(tr("Ort"), m_locationEdit);
    connect(m_locationEdit, &QLineEdit::returnPressed, this, &EventInlineEditor::handleSave);

    m_startEdit = new QDateTimeEdit(this);
    m_startEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd hh:mm"));
    m_startEdit->setCalendarPopup(true);
    formLayout->addRow(tr("Start"), m_startEdit);

    m_endEdit = new QDateTimeEdit(this);
    m_endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd hh:mm"));
    m_endEdit->setCalendarPopup(true);
    formLayout->addRow(tr("Ende"), m_endEdit);
    m_startLabel = qobject_cast<QLabel *>(formLayout->labelForField(m_startEdit));
    m_endLabel = qobject_cast<QLabel *>(formLayout->labelForField(m_endEdit));

    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setPlaceholderText(tr("Beschreibung"));
    formLayout->addRow(tr("Beschreibung"), m_descriptionEdit);
    m_saveLocationShortcut = new QShortcut(QKeySequence(Qt::Key_Return), m_locationEdit);
    m_saveLocationShortcut->setContext(Qt::WidgetShortcut);
    connect(m_saveLocationShortcut, &QShortcut::activated, this, &EventInlineEditor::handleSave);
    m_saveDescriptionShortcut = new QShortcut(QKeySequence(Qt::Key_Return), m_descriptionEdit);
    m_saveDescriptionShortcut->setContext(Qt::WidgetShortcut);
    connect(m_saveDescriptionShortcut, &QShortcut::activated, this, &EventInlineEditor::handleSave);
    auto *descriptionShiftEnter = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return), m_descriptionEdit);
    descriptionShiftEnter->setContext(Qt::WidgetShortcut);
    connect(descriptionShiftEnter, &QShortcut::activated, this, [this]() {
        if (m_descriptionEdit) {
            m_descriptionEdit->insertPlainText(QStringLiteral("\n"));
        }
    });

    layout->addLayout(formLayout);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_cancelButton = new QPushButton(tr("Abbrechen"), this);
    m_saveButton = new QPushButton(tr("Speichern"), this);
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_saveButton);
    layout->addLayout(buttonLayout);

    connect(m_saveButton, &QPushButton::clicked, this, &EventInlineEditor::handleSave);
    connect(m_cancelButton, &QPushButton::clicked, this, &EventInlineEditor::handleCancel);

    m_saveShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Return), this);
    m_saveShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(m_saveShortcut, &QShortcut::activated, this, &EventInlineEditor::handleSave);

    m_escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_escapeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(m_escapeShortcut, &QShortcut::activated, this, &EventInlineEditor::handleSave);
}

void EventInlineEditor::setEvent(const data::CalendarEvent &event)
{
    m_isTodo = false;
    m_event = event;
    m_titleEdit->setText(event.title);
    m_locationEdit->setText(event.location);
    m_startEdit->setDateTime(event.start);
    m_endEdit->setDateTime(event.end);
    m_descriptionEdit->setPlainText(event.description);
    if (m_startLabel) {
        m_startLabel->setText(tr("Start"));
    }
    if (m_endLabel) {
        m_endLabel->setText(tr("Ende"));
        m_endLabel->setVisible(true);
    }
    m_endEdit->setVisible(true);
    setVisible(true);
}

void EventInlineEditor::setTodo(const data::TodoItem &todo)
{
    m_isTodo = true;
    m_todo = todo;
    m_titleEdit->setText(todo.title);
    m_locationEdit->setText(todo.location);
    if (todo.dueDate.isValid()) {
        m_startEdit->setDateTime(todo.dueDate);
    } else {
        m_startEdit->setDateTime(QDateTime::currentDateTime());
    }
    m_endEdit->setVisible(false);
    if (m_endLabel) {
        m_endLabel->setVisible(false);
    }
    if (m_startLabel) {
        m_startLabel->setText(tr("Fällig"));
    }
    m_descriptionEdit->setPlainText(todo.description);
    setVisible(true);
}

void EventInlineEditor::focusTitle(bool selectAll)
{
    if (!m_titleEdit) {
        return;
    }
    m_titleEdit->setFocus(Qt::OtherFocusReason);
    if (selectAll) {
        m_titleEdit->selectAll();
    } else {
        m_titleEdit->setCursorPosition(m_titleEdit->text().length());
    }
}

void EventInlineEditor::clearEditor()
{
    m_event = data::CalendarEvent();
    m_todo = data::TodoItem();
    m_titleEdit->clear();
    m_locationEdit->clear();
    m_descriptionEdit->clear();
    m_startEdit->setDateTime(QDateTime::currentDateTime());
    m_endEdit->setDateTime(QDateTime::currentDateTime().addSecs(30 * 60));
    if (m_startLabel) {
        m_startLabel->setText(tr("Start"));
    }
    if (m_endLabel) {
        m_endLabel->setText(tr("Ende"));
        m_endLabel->setVisible(true);
    }
    m_endEdit->setVisible(true);
    m_isTodo = false;
    setVisible(false);
}

void EventInlineEditor::keyPressEvent(QKeyEvent *event)
{
    if (event && event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        handleSave();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void EventInlineEditor::handleSave()
{
    if (m_isTodo) {
        m_todo.title = m_titleEdit->text();
        m_todo.location = m_locationEdit->text();
        m_todo.dueDate = m_startEdit->dateTime();
        m_todo.description = m_descriptionEdit->toPlainText();
        emit saveTodoRequested(m_todo);
        return;
    }
    m_event.title = m_titleEdit->text();
    m_event.location = m_locationEdit->text();
    m_event.start = m_startEdit->dateTime();
    m_event.end = m_endEdit->dateTime();
    m_event.description = m_descriptionEdit->toPlainText();
    if (!m_event.end.isValid() || m_event.end <= m_event.start) {
        m_event.end = m_event.start.addSecs(30 * 60);
        m_endEdit->setDateTime(m_event.end);
    }
    emit saveRequested(m_event);
}

void EventInlineEditor::handleCancel()
{
    clearEditor();
    emit cancelRequested();
}

} // namespace ui
} // namespace calendar
