#include <QtTest/QtTest>

#include "calendar/core/UndoCommand.hpp"
#include "calendar/core/UndoStack.hpp"

namespace {

class CounterCommand : public calendar::core::UndoCommand
{
public:
    CounterCommand(int delta, int &value)
        : m_delta(delta)
        , m_value(value)
    {
    }

    void redo() override { m_value += m_delta; }
    void undo() override { m_value -= m_delta; }

private:
    int m_delta;
    int &m_value;
};

} // namespace

class UndoStackTest : public QObject
{
    Q_OBJECT

private slots:
    void pushUndoRedo();
    void respectsLimit();
};

void UndoStackTest::pushUndoRedo()
{
    calendar::core::UndoStack stack;
    int value = 0;
    stack.push(std::make_unique<CounterCommand>(5, value));
    QCOMPARE(value, 5);
    QVERIFY(stack.canUndo());

    stack.undo();
    QCOMPARE(value, 0);
    QVERIFY(stack.canRedo());

    stack.redo();
    QCOMPARE(value, 5);
}

void UndoStackTest::respectsLimit()
{
    calendar::core::UndoStack stack(2);
    int value = 0;
    stack.push(std::make_unique<CounterCommand>(1, value));
    stack.push(std::make_unique<CounterCommand>(1, value));
    stack.push(std::make_unique<CounterCommand>(1, value)); // first cmd dropped
    QCOMPARE(stack.count(), static_cast<std::size_t>(2));

    stack.undo();
    stack.undo();
    // First command already dropped, so only two undo operations affect value.
    QCOMPARE(value, 1);
}

QTEST_MAIN(UndoStackTest)
#include "UndoStackTest.moc"
