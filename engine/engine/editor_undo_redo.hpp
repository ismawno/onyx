#pragma once

#include "utils.hpp"
#include "tkit/utils/function.hpp"

#define ONYX_MAX_UNDO_REDO_COMMANDS 256

namespace Engine
{
enum CommandAction : u8
{
    Command_Undo,
    Command_Redo,
};
struct Command
{
    virtual ~Command() = default;

    virtual void Undo() = 0;
    virtual void Redo() = 0;

    void Do(const CommandAction action)
    {
        if (action == Command_Undo)
            Undo();
        else
            Redo();
    }
};

struct LambdaCommand final : Command
{
    TKit::TierFunction<void()> UndoFn;
    TKit::TierFunction<void()> RedoFn;

    LambdaCommand() = default;
    template <typename F1, typename F2>
    LambdaCommand(F1 &&f1, F2 &&f2) : UndoFn(std::forward<F1>(f1)), RedoFn(std::forward<F2>(f2))
    {
    }

    void Undo() override
    {
        UndoFn();
    }
    void Redo() override
    {
        RedoFn();
    }
};

struct UndoRedo
{
    CircularStack<Command *, ONYX_MAX_UNDO_REDO_COMMANDS> UndoStack{};
    CircularStack<Command *, ONYX_MAX_UNDO_REDO_COMMANDS> RedoStack{};

    void Clear();
    void Undo();
    void Redo();

    template <typename T> std::remove_cvref_t<T> *Do(T &&cmd)
    {
        RedoStack.Clear();
        TKit::TierAllocator *tier = TKit::GetTier();
        T *ncmd = tier->CreateWithHeader<std::remove_cvref_t<T>>(std::forward<T>(cmd));
        UndoStack.Push(ncmd);
        return ncmd;
    }
};

} // namespace Engine
