#include <QtTest/QtTest>

#include "calendar/data/InMemoryEventRepository.hpp"
#include "calendar/data/Event.hpp"
#include "calendar/ui/viewmodels/ScheduleViewModel.hpp"

using namespace calendar;

class ScheduleViewModelTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsRange();
};

void ScheduleViewModelTest::loadsRange()
{
    data::InMemoryEventRepository repo;
    data::CalendarEvent event;
    event.title = "Meeting";
    event.start = QDateTime(QDate(2023, 1, 1), QTime(10, 0));
    event.end = event.start.addSecs(3600);
    repo.addEvent(event);

    ui::ScheduleViewModel model(repo);
    model.setRange(QDate(2022, 12, 31), QDate(2023, 1, 2));
    model.refresh();
    QCOMPARE(model.events().size(), static_cast<size_t>(1));
    QCOMPARE(model.events().front().title, QStringLiteral("Meeting"));
}

QTEST_GUILESS_MAIN(ScheduleViewModelTest)
#include "ScheduleViewModelTest.moc"
