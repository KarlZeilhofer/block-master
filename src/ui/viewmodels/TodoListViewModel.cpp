#include "calendar/ui/viewmodels/TodoListViewModel.hpp"

#include <QVector>

#include "calendar/data/TodoRepository.hpp"
#include "calendar/ui/models/TodoListModel.hpp"

namespace calendar {
namespace ui {

TodoListViewModel::TodoListViewModel(data::TodoRepository &repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
    , m_model(std::make_unique<TodoListModel>(this))
{
}

TodoListModel *TodoListViewModel::model() const
{
    return m_model.get();
}

void TodoListViewModel::refresh()
{
    const auto todos = m_repository.fetchTodos();
    QVector<data::TodoItem> items;
    items.reserve(static_cast<int>(todos.size()));
    for (const auto &todo : todos) {
        items.append(todo);
    }
    m_model->setTodos(std::move(items));
    emit todosChanged();
}

} // namespace ui
} // namespace calendar
