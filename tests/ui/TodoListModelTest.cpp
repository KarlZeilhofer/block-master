#include <QtTest/QtTest>

#include "calendar/ui/models/TodoListModel.hpp"

using namespace calendar;

class TodoListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void setTodosAndData();
    void mimeDataContainsIds();
};

void TodoListModelTest::setTodosAndData()
{
    ui::TodoListModel model;
    QVector<data::TodoItem> todos;
    data::TodoItem todo;
    todo.title = "Item 1";
    todo.description = "Desc";
    todos.append(todo);
    model.setTodos(todos);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole), QVariant(QStringLiteral("Item 1")));
    QCOMPARE(model.data(model.index(0, 0), Qt::ToolTipRole), QVariant(QStringLiteral("Desc")));
}

void TodoListModelTest::mimeDataContainsIds()
{
    ui::TodoListModel model;
    QVector<data::TodoItem> todos;
    data::TodoItem todo;
    todo.title = "Item A";
    todo.id = QUuid::createUuid();
    todos.append(todo);
    model.setTodos(todos);

    const auto indexes = QModelIndexList{ model.index(0, 0) };
    std::unique_ptr<QMimeData> mime(model.mimeData(indexes));
    QVERIFY(mime->hasFormat(QStringLiteral("application/x-calendar-todo")));
}

QTEST_GUILESS_MAIN(TodoListModelTest)
#include "TodoListModelTest.moc"
