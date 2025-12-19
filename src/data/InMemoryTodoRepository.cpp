#include "calendar/data/InMemoryTodoRepository.hpp"

namespace calendar {
namespace data {

InMemoryTodoRepository::InMemoryTodoRepository() = default;
InMemoryTodoRepository::~InMemoryTodoRepository() = default;

std::vector<TodoItem> InMemoryTodoRepository::fetchTodos() const
{
    std::vector<TodoItem> todos;
    todos.reserve(static_cast<size_t>(m_items.size()));
    for (const auto &item : m_items) {
        todos.push_back(item);
    }
    return todos;
}

std::optional<TodoItem> InMemoryTodoRepository::findById(const QUuid &id) const
{
    if (m_items.contains(id)) {
        return m_items.value(id);
    }
    return std::nullopt;
}

TodoItem InMemoryTodoRepository::addTodo(TodoItem todo)
{
    if (todo.id.isNull()) {
        todo.id = QUuid::createUuid();
    }
    m_items.insert(todo.id, todo);
    return todo;
}

bool InMemoryTodoRepository::updateTodo(const TodoItem &todo)
{
    if (!m_items.contains(todo.id)) {
        return false;
    }
    m_items.insert(todo.id, todo);
    return true;
}

bool InMemoryTodoRepository::removeTodo(const QUuid &id)
{
    return m_items.remove(id) > 0;
}

} // namespace data
} // namespace calendar
