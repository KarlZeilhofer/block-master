#include "calendar/ui/dialogs/EventDetailDialog.hpp"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace calendar {
namespace ui {

namespace {
QString recurrenceToString(const QString &rule)
{
    if (rule == QStringLiteral("DAILY")) {
        return QObject::tr("Täglich");
    }
    if (rule == QStringLiteral("WEEKLY")) {
        return QObject::tr("Wöchentlich");
    }
    if (rule == QStringLiteral("MONTHLY")) {
        return QObject::tr("Monatlich");
    }
    if (rule == QStringLiteral("YEARLY")) {
        return QObject::tr("Jährlich");
    }
    return QObject::tr("Keine");
}

QString stringToRule(const QString &display)
{
    if (display.contains("Täglich")) {
        return QStringLiteral("DAILY");
    }
    if (display.contains("Wöchentlich")) {
        return QStringLiteral("WEEKLY");
    }
    if (display.contains("Monatlich")) {
        return QStringLiteral("MONTHLY");
    }
    if (display.contains("Jährlich")) {
        return QStringLiteral("YEARLY");
    }
    return {};
}
} // namespace

EventDetailDialog::EventDetailDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Termin bearbeiten"));
    auto *layout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout();

    m_titleEdit = new QLineEdit(this);
    formLayout->addRow(tr("Titel"), m_titleEdit);

    m_locationEdit = new QLineEdit(this);
    formLayout->addRow(tr("Ort"), m_locationEdit);

    m_startEdit = new QDateTimeEdit(this);
    m_startEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd hh:mm"));
    m_startEdit->setCalendarPopup(true);
    formLayout->addRow(tr("Start"), m_startEdit);

    m_endEdit = new QDateTimeEdit(this);
    m_endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd hh:mm"));
    m_endEdit->setCalendarPopup(true);
    formLayout->addRow(tr("Ende"), m_endEdit);

    m_descriptionEdit = new QPlainTextEdit(this);
    formLayout->addRow(tr("Beschreibung"), m_descriptionEdit);

    m_recurrenceCombo = new QComboBox(this);
    m_recurrenceCombo->addItems({ tr("Keine"), tr("Täglich"), tr("Wöchentlich"), tr("Monatlich"), tr("Jährlich") });
    formLayout->addRow(tr("Wiederholung"), m_recurrenceCombo);

    m_reminderSpin = new QSpinBox(this);
    m_reminderSpin->setRange(0, 1440);
    m_reminderSpin->setSuffix(tr(" Minuten vorher"));
    formLayout->addRow(tr("Erinnerung"), m_reminderSpin);

    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->setEditable(true);
    m_categoryCombo->addItems({ tr("Allgemein"), tr("Meeting"), tr("Privat"), tr("Reise") });
    formLayout->addRow(tr("Kategorie"), m_categoryCombo);

    layout->addLayout(formLayout);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

void EventDetailDialog::setEvent(const data::CalendarEvent &event)
{
    m_event = event;
    m_titleEdit->setText(event.title);
    m_locationEdit->setText(event.location);
    m_descriptionEdit->setPlainText(event.description);
    m_startEdit->setDateTime(event.start);
    m_endEdit->setDateTime(event.end);
    m_reminderSpin->setValue(event.reminderMinutes);
    m_recurrenceCombo->setCurrentText(recurrenceToString(event.recurrenceRule));
    if (!event.categories.isEmpty()) {
        m_categoryCombo->setCurrentText(event.categories.first());
    }
}

data::CalendarEvent EventDetailDialog::event() const
{
    data::CalendarEvent updated = m_event;
    updated.title = m_titleEdit->text();
    updated.location = m_locationEdit->text();
    updated.description = m_descriptionEdit->toPlainText();
    updated.start = m_startEdit->dateTime();
    updated.end = m_endEdit->dateTime();
    if (updated.end <= updated.start) {
        updated.end = updated.start.addSecs(30 * 60);
    }
    updated.reminderMinutes = m_reminderSpin->value();
    updated.recurrenceRule = stringToRule(m_recurrenceCombo->currentText());
    updated.categories = QStringList{ m_categoryCombo->currentText() };
    return updated;
}

} // namespace ui
} // namespace calendar
