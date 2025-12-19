#pragma once

#include <QHash>

#include "calendar/data/TodoRepository.hpp"

namespace calendar {
namespace data {

class InMemoryTodoRepository : public TodoRepository
{
public:
    InMemoryTodoRepository();
    ~InMemoryTodoRepository() override;

    std::vector<TodoItem> fetchTodos() const override;
    std::optional<TodoItem> findById(const QUuid &id) const override;
    TodoItem addTodo(TodoItem todo) override;
    bool updateTodo(const TodoItem &todo) override;
    bool removeTodo(const QUuid &id) override;

private:
    QHash<QUuid, TodoItem> m_items;
};

} // namespace data
} // namespace calendar
