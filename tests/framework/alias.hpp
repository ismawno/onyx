#pragma once

// The containers onyx-tests is written with, named once. Everything under tests/ says String, Array<T>, Path and
// Function<S>: no file names a Toolkit allocation, so the two lines below are all there is to change to move the whole
// suite to another one (TKit::DynamicAllocation and TKit::DynamicStorage, say).
//
// The default is Toolkit's tier allocator, the one Onyx itself hands out: what Onyx returns (an error message, a
// serialization path) and what it takes (a file path) is then the same type the tests use, with no conversions.

#include "onyx/alias.hpp"
#include "tkit/container/tier_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/filesystem/path.hpp"
#include "tkit/utils/function.hpp"

namespace OnyxTest
{
using namespace TKit::Alias;

template <typename T> using Allocation = TKit::TierAllocation<T>;
using Storage = TKit::TierStorage;

template <typename T> using Array = TKit::Array<T, Allocation<T>>;
using String = TKit::String<Allocation<char>>;
using Path = TKit::Path<Allocation<char>>;
using StringView = TKit::StringView;
template <typename Signature> using Function = TKit::Function<Storage, Signature>;

// Tier containers take their memory from the tier allocator on top of the calling thread's stack of them, and Toolkit
// keeps one stack per thread. Onyx pushes its own on the main thread, between Initialize and Terminate. onyx-tests
// needs strings outside of that (the supervisor never initializes Onyx, a worker parses its options before Initialize
// and reports after Terminate) and on the threads it starts, so every thread of onyx-tests calls this first: it gives
// the thread an allocator of its own, under whatever Onyx pushes later. Calling it again does nothing.
//
// A tier allocator has a largest allocation it can serve and a fixed number of slots, and it is not thread safe: a
// container filled on one thread may be read and destroyed by another only once the first one is done with its
// allocator (the supervisor reads what its job threads collected after they finished). maxAllocation is that largest
// allocation, a power of two. Only the first call of a process sets it
void PushThreadAllocator(usz maxAllocation);

// calls part(StringView) for every piece of a text between delimiters, as String::Split cuts it, but without a string
// per piece: Split reserves room for one string per character before it looks for delimiters, which is more than a
// tier allocator can serve for a long text. This allocates nothing
template <typename F> void ForEachPart(const StringView text, const char delimiter, F &&part)
{
    usize start = 0;
    for (usize i = 0; i <= text.GetSize(); ++i)
        if (i == text.GetSize() || text[i] == delimiter)
        {
            part(StringView{text.GetData() + start, i - start});
            start = i + 1;
        }
}
// the lines of a text, without their '\n'. a text that ends in one has no empty last line
template <typename F> void ForEachLine(const StringView text, F &&line)
{
    ForEachPart(text, '\n', [&](const StringView part) {
        if (!part.IsEmpty() || part.GetData() != text.GetData() + text.GetSize())
            line(part);
    });
}
} // namespace OnyxTest
