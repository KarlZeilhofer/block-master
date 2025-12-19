#pragma once

namespace calendar {
namespace core {

class UndoCommand
{
public:
    virtual ~UndoCommand() = default;
    virtual void redo() = 0;
    virtual void undo() = 0;
};

} // namespace core
} // namespace calendar
