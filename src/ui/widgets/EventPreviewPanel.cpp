#include "calendar/ui/widgets/EventPreviewPanel.hpp"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPalette>

namespace calendar {
namespace ui {

EventPreviewPanel::EventPreviewPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    m_titleLabel = new QLabel(tr("Kein Termin ausgewählt"), this);
    m_titleLabel->setObjectName(QStringLiteral("previewTitle"));
    auto font = m_titleLabel->font();
    font.setBold(true);
    m_titleLabel->setFont(font);
    layout->addWidget(m_titleLabel);

    m_timeLabel = new QLabel(this);
    layout->addWidget(m_timeLabel);

    m_locationLabel = new QLabel(this);
    layout->addWidget(m_locationLabel);

    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setWordWrap(true);
    layout->addWidget(m_descriptionLabel);

    m_hintLabel = new QLabel(tr("Shortcuts: ⏎ Details • E Inline • Ctrl+C Copy • Ctrl+V Paste • Del Löschen • Space Info"), this);
    QPalette pal = m_hintLabel->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::Disabled, QPalette::WindowText));
    m_hintLabel->setPalette(pal);
    layout->addWidget(m_hintLabel);

    setVisible(false);
}

void EventPreviewPanel::setEvent(const data::CalendarEvent &event)
{
    m_titleLabel->setText(event.title.isEmpty() ? tr("(Ohne Titel)") : event.title);
    const QString timeText = tr("%1 – %2")
                                 .arg(event.start.toString(QStringLiteral("ddd, dd.MM. hh:mm")),
                                      event.end.toString(QStringLiteral("hh:mm")));
    m_timeLabel->setText(timeText);

    m_locationLabel->setText(event.location.isEmpty() ? tr("Ort: –") : tr("Ort: %1").arg(event.location));

    QString desc = event.description;
    if (desc.length() > 140) {
        desc = desc.left(137) + QStringLiteral("…");
    }
    m_descriptionLabel->setText(desc.isEmpty() ? tr("Keine Beschreibung") : desc);
    setVisible(true);
}

void EventPreviewPanel::setTodo(const data::TodoItem &todo)
{
    m_titleLabel->setText(todo.title.isEmpty() ? tr("(Ohne Titel)") : todo.title);
    if (todo.dueDate.isValid()) {
        m_timeLabel->setText(tr("Fällig: %1").arg(todo.dueDate.toString(QStringLiteral("ddd, dd.MM. hh:mm"))));
    } else {
        m_timeLabel->setText(tr("Fällig: –"));
    }
    m_locationLabel->setText(todo.location.isEmpty() ? tr("Ort: –") : tr("Ort: %1").arg(todo.location));
    QString desc = todo.description;
    if (desc.length() > 140) {
        desc = desc.left(137) + QStringLiteral("…");
    }
    m_descriptionLabel->setText(desc.isEmpty() ? tr("Keine Beschreibung") : desc);
    setVisible(true);
}

void EventPreviewPanel::clearPreview()
{
    m_titleLabel->setText(tr("Kein Termin ausgewählt"));
    m_timeLabel->clear();
    m_locationLabel->clear();
    m_descriptionLabel->clear();
    setVisible(false);
}

} // namespace ui
} // namespace calendar
