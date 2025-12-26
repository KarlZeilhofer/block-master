#pragma once

#include <QDialog>
#include <QColor>

class QListWidget;
class QStackedWidget;
class QPlainTextEdit;
class QColorDialog;
class QLabel;

namespace calendar {
namespace ui {

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private:
    void setupUi();
    QWidget *createCalendarPage();
    QWidget *createKeywordPage();
    void updateColorFromCursor();
    void applyColorToCurrentLine(const QColor &color);
    QString currentLineText() const;
    QColor colorFromLine(const QString &line) const;

    QListWidget *m_categoryList = nullptr;
    QStackedWidget *m_pages = nullptr;
    QPlainTextEdit *m_keywordEditor = nullptr;
    QColorDialog *m_colorDialog = nullptr;
    bool m_updatingFromEditor = false;
    bool m_updatingFromPicker = false;
};

} // namespace ui
} // namespace calendar
