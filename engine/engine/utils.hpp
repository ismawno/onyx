#pragma once

#include "alias.hpp"
#include "tkit/utils/bit.hpp"
#include "tkit/container/hive.hpp"
#include "tkit/container/stack_array.hpp"

namespace Engine
{
template <typename T, u32 N> class CircularStack
{
    static_assert(TKit::IsPowerOfTwo(N), "[ONYX][C-STACK] N must be a power of 2");

  public:
    void Clear()
    {
        m_Size = 0;
        m_Cursor = 0;
    }

    void Push(const T &elm)
    {
        m_Stack[m_Cursor] = elm;
        m_Cursor = normalizeIndex(m_Cursor + 1);
        if (m_Size < N)
            ++m_Size;
    }
    T Pop()
    {
        const T cmd = m_Stack[--m_Cursor];
        --m_Size;
        m_Stack[m_Cursor] = T{};
        return cmd;
    }

    template <typename F> void Each(F func)
    {
        for (u32 i = 0; i < m_Size; ++i)
            func(m_Stack[normalizeIndex(m_Cursor + i)]);
    }

    bool IsEmpty() const
    {
        return m_Size == 0;
    }

    const T &operator[](const u32 idx) const
    {
        return m_Stack[idx];
    }
    T &operator[](const u32 idx)
    {
        return m_Stack[idx];
    }

  private:
    static u32 normalizeIndex(const u32 idx)
    {
        return idx & (N - 1);
    }
    TKit::FixedArray<T, N> m_Stack{};
    u32 m_Cursor = 0;
    u32 m_Size = 0;
};

struct NameArray
{
    TKit::StackArray<TKit::StackString> Names{};
    TKit::TierArray<u32> Ids{};

    u32 GetId(const u32 idx) const
    {
        return idx < Ids.GetSize() ? Ids[idx] : TKIT_U32_MAX;
    }
};

template <typename T> NameArray CreateNameArray(const TKit::TierHive<T> &elements)
{
    NameArray array{};
    array.Names.Reserve(elements.GetSize());
    array.Ids.Reserve(elements.GetSize());

    elements.IterateByInsertionOrder([&](const u32 id, const T &element) {
        array.Names.Append(element.Name);
        array.Ids.Append(id);
    });
    return array;
}

template <typename T> TKit::StackArray<u32> CreateIdArray(const TKit::TierHive<T> &elements)
{
    TKit::StackArray<u32> ids{};
    ids.Reserve(elements.GetSize());

    elements.IterateByInsertionOrder([&](const u32 id) { ids.Append(id); });
    return ids;
}

template <typename T> TKit::StackArray<TKit::StackString> CreateNameArray(const TKit::Span<const T> elements)
{
    TKit::StackArray<TKit::StackString> names{};
    names.Reserve(elements.GetSize());
    for (const T &elm : elements)
        names.Append(elm.Name);
    return names;
}

TKit::TierPath GetHomePath();
TKit::TierPath GetConfigPath();

ONYX_NO_DISCARD Result<TKit::StackArray<TKit::TierPath>> LoadRecentProjects();
ONYX_NO_DISCARD Result<> UpdateRecentProjects(const TKit::TierPath &path);
} // namespace Engine
