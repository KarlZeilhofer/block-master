#include "calendar/ui/dialogs/SettingsDialog.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace calendar {
namespace ui {

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Einstellungen"));
    resize(720, 480);
    setupUi();
}

void SettingsDialog::setupUi()
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_categoryList = new QListWidget(this);
    m_categoryList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_categoryList->setFixedWidth(220);
    m_categoryList->addItem(tr("Kalender"));
    m_categoryList->addItem(tr("Schlüsselwörter"));

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(createCalendarPage());
    m_pages->addWidget(createKeywordPage());

    layout->addWidget(m_categoryList);
    layout->addWidget(m_pages, 1);

    connect(m_categoryList, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_categoryList->setCurrentRow(0);
}

QWidget *SettingsDialog::createCalendarPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("Kalendereinstellungen"), page);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *hint = new QLabel(tr("Hier können zukünftig Kalendereinstellungen vorgenommen werden."), page);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch(1);
    return page;
}

QWidget *SettingsDialog::createKeywordPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("Schlüsselwörter"), page);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *hint = new QLabel(tr("Hier lassen sich später Schlüsselwort-Definitionen mit Farben pflegen."), page);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch(1);
    return page;
}

} // namespace ui
} // namespace calendar
