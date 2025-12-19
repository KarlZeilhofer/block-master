#pragma once

#include <optional>
#include <vector>

#include "calendar/data/Todo.hpp"

namespace calendar {
namespace data {

class TodoRepository
{
public:
    virtual ~TodoRepository() = default;

    virtual std::vector<TodoItem> fetchTodos() const = 0;
    virtual std::optional<TodoItem> findById(const QUuid &id) const = 0;
    virtual TodoItem addTodo(TodoItem todo) = 0;
    virtual bool updateTodo(const TodoItem &todo) = 0;
    virtual bool removeTodo(const QUuid &id) = 0;
};

} // namespace data
} // namespace calendar
