#include <QtTest/QtTest>

#include "calendar/data/InMemoryTodoRepository.hpp"

using namespace calendar::data;

class TodoRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void addAndFetch();
    void updateAndRemove();
};

void TodoRepositoryTest::addAndFetch()
{
    InMemoryTodoRepository repo;
    TodoItem todo;
    todo.title = "Test";
    todo.priority = 3;
    const auto stored = repo.addTodo(todo);

    QVERIFY(!stored.id.isNull());

    const auto list = repo.fetchTodos();
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.front().title, QStringLiteral("Test"));
    QCOMPARE(list.front().priority, 3);

    const auto fetched = repo.findById(stored.id);
    QVERIFY(fetched.has_value());
    QCOMPARE(fetched->title, QStringLiteral("Test"));
}

void TodoRepositoryTest::updateAndRemove()
{
    InMemoryTodoRepository repo;
    TodoItem todo;
    todo.title = "Initial";
    const auto stored = repo.addTodo(todo);

    TodoItem toUpdate = stored;
    toUpdate.title = "Updated";
    QVERIFY(repo.updateTodo(toUpdate));

    const auto fetched = repo.findById(stored.id);
    QVERIFY(fetched.has_value());
    QCOMPARE(fetched->title, QStringLiteral("Updated"));

    QVERIFY(repo.removeTodo(stored.id));
    QVERIFY(!repo.findById(stored.id).has_value());
}

QTEST_MAIN(TodoRepositoryTest)
#include "TodoRepositoryTest.moc"
