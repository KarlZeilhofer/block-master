#include "calendar/ui/dialogs/SettingsDialog.hpp"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextBlock>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QTextCursor>
#include <QSignalBlocker>

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
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *editorContainer = new QWidget(page);
    auto *editorLayout = new QVBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(6);

    auto *title = new QLabel(tr("Schlüsselwörter (ein Eintrag pro Zeile, z. B. \"Urlaub #8080ff\")"), page);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    editorLayout->addWidget(title);

    m_keywordEditor = new QPlainTextEdit(page);
    m_keywordEditor->setPlaceholderText(tr("Urlaub #8080ff\nDeadline #ff3030"));
    editorLayout->addWidget(m_keywordEditor, 1);

    auto *hint = new QLabel(tr("Wähle rechts eine Farbe aus, sie wird automatisch für die aktuelle Zeile übernommen."), page);
    hint->setWordWrap(true);
    editorLayout->addWidget(hint);

    layout->addWidget(editorContainer, 1);

    m_colorDialog = new QColorDialog(page);
    m_colorDialog->setOption(QColorDialog::NoButtons, true);
    m_colorDialog->setOption(QColorDialog::ShowAlphaChannel, false);
    m_colorDialog->setWindowFlags(m_colorDialog->windowFlags() & ~Qt::Dialog);
    m_colorDialog->setFocusPolicy(Qt::NoFocus);
    const auto colorChildren = m_colorDialog->findChildren<QWidget *>();
    for (auto *child : colorChildren) {
        child->setFocusPolicy(Qt::NoFocus);
    }
    layout->addWidget(m_colorDialog);

    connect(m_keywordEditor, &QPlainTextEdit::cursorPositionChanged, this, &SettingsDialog::updateColorFromCursor);
    connect(m_keywordEditor, &QPlainTextEdit::textChanged, this, &SettingsDialog::updateColorFromCursor);
    connect(m_colorDialog, &QColorDialog::currentColorChanged, this, &SettingsDialog::applyColorToCurrentLine);

    updateColorFromCursor();
    return page;
}

void SettingsDialog::setKeywordText(const QString &text)
{
    if (!m_keywordEditor) {
        return;
    }
    m_keywordEditor->setPlainText(text);
    QTextCursor cursor = m_keywordEditor->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_keywordEditor->setTextCursor(cursor);
    updateColorFromCursor();
}

QString SettingsDialog::keywordText() const
{
    if (!m_keywordEditor) {
        return {};
    }
    return m_keywordEditor->toPlainText();
}

void SettingsDialog::updateColorFromCursor()
{
    if (!m_keywordEditor || !m_colorDialog) {
        return;
    }
    if (m_updatingFromPicker) {
        return;
    }
    const QString line = currentLineText();
    QColor color = colorFromLine(line);
    if (!color.isValid()) {
        color = QColor(Qt::white);
    }
    const bool editorHadFocus = m_keywordEditor->hasFocus();
    m_updatingFromEditor = true;
    m_colorDialog->setCurrentColor(color);
    m_updatingFromEditor = false;
    if (editorHadFocus && !m_keywordEditor->hasFocus()) {
        m_keywordEditor->setFocus(Qt::OtherFocusReason);
    }
}

void SettingsDialog::applyColorToCurrentLine(const QColor &color)
{
    if (!m_keywordEditor || !color.isValid()) {
        return;
    }
    if (m_updatingFromEditor) {
        return;
    }
    m_updatingFromPicker = true;
    QTextCursor cursor = m_keywordEditor->textCursor();
    int column = cursor.positionInBlock();
    const QString originalLine = cursor.block().text();
    QString updatedLine = originalLine;
    const QString colorCode = color.name(QColor::HexRgb).toLower();
    QRegularExpression regex(QStringLiteral("#[0-9a-fA-F]{6}"));
    if (regex.match(updatedLine).hasMatch()) {
        updatedLine.replace(regex, colorCode);
    } else {
        if (updatedLine.trimmed().isEmpty()) {
            updatedLine = colorCode;
        } else if (updatedLine.endsWith(' ') || updatedLine.endsWith('\t')) {
            updatedLine += colorCode;
        } else {
            updatedLine += QStringLiteral(" %1").arg(colorCode);
        }
    }
    {
        QSignalBlocker blocker(m_keywordEditor);
        cursor.select(QTextCursor::LineUnderCursor);
        cursor.insertText(updatedLine);
    }
    QTextCursor restoreCursor = m_keywordEditor->textCursor();
    restoreCursor.movePosition(QTextCursor::StartOfBlock);
    const int targetColumn = qMin(column, updatedLine.length());
    restoreCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, targetColumn);
    m_keywordEditor->setTextCursor(restoreCursor);
    m_updatingFromPicker = false;
}

QString SettingsDialog::currentLineText() const
{
    if (!m_keywordEditor) {
        return {};
    }
    QTextCursor cursor = m_keywordEditor->textCursor();
    return cursor.block().text();
}

QColor SettingsDialog::colorFromLine(const QString &line) const
{
    QRegularExpression regex(QStringLiteral("#([0-9a-fA-F]{6})"));
    QRegularExpressionMatch match = regex.match(line);
    if (!match.hasMatch()) {
        return QColor();
    }
    const QString code = QStringLiteral("#%1").arg(match.captured(1));
    QColor color(code);
    return color;
}

} // namespace ui
} // namespace calendar
