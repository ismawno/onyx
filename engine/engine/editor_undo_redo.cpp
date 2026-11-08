#include "editor_undo_redo.hpp"

namespace Engine
{
using Stack = CircularStack<Command *, EDITOR_MAX_UNDO_REDO_COMMANDS>;
static void clearStack(Stack &cmds)
{
    TKit::TierAllocator *tier = TKit::GetTier();
    for (u32 i = 0; i < EDITOR_MAX_UNDO_REDO_COMMANDS; ++i)
        if (cmds[i])
        {
            tier->DeallocateWithHeader(cmds[i]);
            cmds[i] = nullptr;
        }
}

static void pushPop(Stack &pusher, Stack &popper, const CommandAction action)
{
    if (popper.IsEmpty())
        return;

    Command *cmd = popper.Pop();
    cmd->Do(action);
    pusher.Push(cmd);
}

void UndoRedo::Clear()
{
    clearStack(UndoStack);
    clearStack(RedoStack);

    UndoStack.Clear();
    RedoStack.Clear();
}

void UndoRedo::Undo()
{
    pushPop(RedoStack, UndoStack, Command_Undo);
}
void UndoRedo::Redo()
{
    pushPop(UndoStack, RedoStack, Command_Redo);
}
} // namespace Engine
