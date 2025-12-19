#pragma once

#include <memory>

namespace calendar {
namespace data {
class DataProvider;
class TodoRepository;
class EventRepository;
}

namespace core {

class UndoStack;

class AppContext
{
public:
    AppContext();
    ~AppContext();

    data::TodoRepository &todoRepository();
    data::EventRepository &eventRepository();
    UndoStack &undoStack();

private:
    std::unique_ptr<data::DataProvider> m_dataProvider;
    std::unique_ptr<UndoStack> m_undoStack;
};

} // namespace core
} // namespace calendar
