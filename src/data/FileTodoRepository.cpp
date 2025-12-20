#include "calendar/data/FileTodoRepository.hpp"

#include <algorithm>

namespace calendar {
namespace data {

FileTodoRepository::FileTodoRepository(std::shared_ptr<FileCalendarStorage> storage)
    : m_storage(std::move(storage))
{
}

std::vector<TodoItem> FileTodoRepository::fetchTodos() const
{
    std::vector<TodoItem> result;
    if (!m_storage) {
        return result;
    }
    const auto &todos = m_storage->todos();
    result.reserve(static_cast<size_t>(todos.size()));
    for (auto it = todos.constBegin(); it != todos.constEnd(); ++it) {
        result.push_back(it.value());
    }
    std::sort(result.begin(), result.end(), [](const TodoItem &lhs, const TodoItem &rhs) {
        if (lhs.priority == rhs.priority) {
            return lhs.title.toLower() < rhs.title.toLower();
        }
        return lhs.priority > rhs.priority;
    });
    return result;
}

std::optional<TodoItem> FileTodoRepository::findById(const QUuid &id) const
{
    if (!m_storage) {
        return std::nullopt;
    }
    const auto &todos = m_storage->todos();
    if (todos.contains(id)) {
        return todos.value(id);
    }
    return std::nullopt;
}

TodoItem FileTodoRepository::addTodo(TodoItem todo)
{
    if (!m_storage) {
        return todo;
    }
    return m_storage->addOrUpdateTodo(std::move(todo));
}

bool FileTodoRepository::updateTodo(const TodoItem &todo)
{
    if (!m_storage) {
        return false;
    }
    const auto &todos = m_storage->todos();
    if (!todos.contains(todo.id)) {
        return false;
    }
    m_storage->addOrUpdateTodo(todo);
    return true;
}

bool FileTodoRepository::removeTodo(const QUuid &id)
{
    if (!m_storage) {
        return false;
    }
    return m_storage->removeTodo(id);
}

} // namespace data
} // namespace calendar

