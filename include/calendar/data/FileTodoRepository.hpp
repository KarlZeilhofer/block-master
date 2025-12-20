#pragma once

#include "calendar/data/TodoRepository.hpp"
#include "calendar/data/FileCalendarStorage.hpp"

#include <memory>

namespace calendar {
namespace data {

class FileTodoRepository : public TodoRepository
{
public:
    explicit FileTodoRepository(std::shared_ptr<FileCalendarStorage> storage);
    ~FileTodoRepository() override = default;

    std::vector<TodoItem> fetchTodos() const override;
    std::optional<TodoItem> findById(const QUuid &id) const override;
    TodoItem addTodo(TodoItem todo) override;
    bool updateTodo(const TodoItem &todo) override;
    bool removeTodo(const QUuid &id) override;

private:
    std::shared_ptr<FileCalendarStorage> m_storage;
};

} // namespace data
} // namespace calendar

