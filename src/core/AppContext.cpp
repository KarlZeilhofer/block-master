#include "calendar/core/AppContext.hpp"

#include "calendar/data/DataProvider.hpp"

#include "calendar/core/UndoStack.hpp"

namespace calendar {
namespace core {

AppContext::AppContext()
    : m_dataProvider(std::make_unique<data::DataProvider>())
    , m_undoStack(std::make_unique<UndoStack>())
{
}

AppContext::~AppContext() = default;

data::TodoRepository &AppContext::todoRepository()
{
    return m_dataProvider->todoRepository();
}

data::EventRepository &AppContext::eventRepository()
{
    return m_dataProvider->eventRepository();
}

UndoStack &AppContext::undoStack()
{
    return *m_undoStack;
}

} // namespace core
} // namespace calendar
