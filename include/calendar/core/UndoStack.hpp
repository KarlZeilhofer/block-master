#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace calendar {
namespace core {

class UndoCommand;

class UndoStack
{
public:
    explicit UndoStack(std::size_t limit = 100);
    ~UndoStack();

    void push(std::unique_ptr<UndoCommand> command);
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    void clear();
    std::size_t count() const;

private:
    std::vector<std::unique_ptr<UndoCommand>> m_commands;
    std::size_t m_index = 0;
    std::size_t m_limit = 0;
};

} // namespace core
} // namespace calendar
