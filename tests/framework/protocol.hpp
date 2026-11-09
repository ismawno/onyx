#pragma once

// The line protocol of onyx-tests: workers report to the supervisor with it, and the supervisor reports to tools with
// it when asked to (--format events, which is what the control center reads).
//
// One event a line: the marker, the kind of event, then its fields, all separated by tabs. Fields are escaped (\\, \t,
// \n and \r become two characters), so they carry any text: a Catch2 expansion that spans several lines, a path with
// tabs in it.
//
// worker to supervisor          supervisor to tools (--format events)
//   fail  where expr exp info     test     id suite description known-issue file line
//   skip  reason                  plan     count
//   note  text                    begin    id
//   end   status milliseconds     fail     id where expression expansion info
//                                 skip     id reason
//                                 note     id text
//                                 crash    id what
//                                 output   id line
//                                 end      id status milliseconds
//                                 summary  pass fail skip crash timeout known milliseconds

#include "onyx/alias.hpp"
#include "tkit/container/dynamic_array.hpp"
#include <cstring>
#include <string>

namespace OnyxTest
{
using namespace TKit::Alias;

constexpr const char *ProtocolMarker = "@onyx-test";

namespace Detail
{
void AppendEscaped(TKit::DynamicString &line, const char *data, usz size);
inline void AppendField(TKit::DynamicString &line, const char *text)
{
    AppendEscaped(line, text, std::strlen(text));
}
inline void AppendField(TKit::DynamicString &line, const TKit::DynamicString &text)
{
    AppendEscaped(line, text.GetData(), text.GetSize());
}
inline void AppendField(TKit::DynamicString &line, const std::string &text)
{
    AppendEscaped(line, text.data(), text.size());
}
void WriteLine(const TKit::DynamicString &line);
} // namespace Detail

// writes one event to stdout as a whole line and flushes it: events from several threads never interleave
template <typename... Fields> void Emit(const char *kind, const Fields &...fields)
{
    TKit::DynamicString line{ProtocolMarker};
    line += '\t';
    line += kind;
    ((line += '\t', Detail::AppendField(line, fields)), ...);
    Detail::WriteLine(line);
}

// the fields of an event, the kind first, unescaped. false if the line holds no event. a log line written by another
// thread right before the event, on the same line, is skipped
bool ParseEvent(const TKit::DynamicString &line, TKit::DynamicArray<TKit::DynamicString> &fields);

// paths in reports are relative to the repository, so CI annotations land on the right file
TKit::DynamicString RelativePath(const char *file);
} // namespace OnyxTest
