#pragma once

#include <QMainWindow>
#include <memory>

namespace calendar {
namespace core {
class AppContext;
}

namespace ui {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();
    std::unique_ptr<core::AppContext> m_appContext;
};

} // namespace ui
} // namespace calendar
