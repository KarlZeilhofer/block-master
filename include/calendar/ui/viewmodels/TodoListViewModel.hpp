#pragma once

#include <QObject>
#include <memory>

#include "calendar/data/Todo.hpp"

namespace calendar {
namespace data {
class TodoRepository;
}

namespace ui {

class TodoListModel;

class TodoListViewModel : public QObject
{
    Q_OBJECT
public:
    TodoListViewModel(data::TodoRepository &repository, QObject *parent = nullptr);

    TodoListModel *model() const;

public slots:
    void refresh();

signals:
    void todosChanged();

private:
    data::TodoRepository &m_repository;
    std::unique_ptr<TodoListModel> m_model;
};

} // namespace ui
} // namespace calendar
