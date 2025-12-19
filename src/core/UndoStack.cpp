#include "calendar/core/UndoStack.hpp"

#include "calendar/core/UndoCommand.hpp"

namespace calendar {
namespace core {

UndoStack::UndoStack(std::size_t limit)
    : m_limit(limit)
{
    m_commands.reserve(limit);
}

UndoStack::~UndoStack() = default;

void UndoStack::push(std::unique_ptr<UndoCommand> command)
{
    if (!command) {
        return;
    }
    // Remove redoable commands past current index.
    if (m_index < m_commands.size()) {
        m_commands.erase(m_commands.begin() + static_cast<long>(m_index), m_commands.end());
    }

    if (m_commands.size() == m_limit) {
        m_commands.erase(m_commands.begin());
        if (m_index > 0) {
            --m_index;
        }
    }

    command->redo();
    m_commands.push_back(std::move(command));
    m_index = m_commands.size();
}

bool UndoStack::canUndo() const
{
    return m_index > 0;
}

bool UndoStack::canRedo() const
{
    return m_index < m_commands.size();
}

void UndoStack::undo()
{
    if (!canUndo()) {
        return;
    }
    auto &command = m_commands[m_index - 1];
    command->undo();
    --m_index;
}

void UndoStack::redo()
{
    if (!canRedo()) {
        return;
    }
    auto &command = m_commands[m_index];
    command->redo();
    ++m_index;
}

void UndoStack::clear()
{
    m_commands.clear();
    m_index = 0;
}

std::size_t UndoStack::count() const
{
    return m_commands.size();
}

} // namespace core
} // namespace calendar
