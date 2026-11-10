#pragma once

// The line protocol of onyx-tests: workers report to the supervisor with it, and the supervisor reports to tools with
// it when asked to (--format events, which is what the control center reads).
//
// One event a line: the marker, the kind of event, then its fields, all separated by tabs. Fields are escaped (\\, \t,
// \n and \r become two characters), so they carry any text: a Catch2 expansion that spans several lines, a path with
// tabs in it.
//
// worker to supervisor              supervisor to tools (--format events)
//   begin     id                    test     id suite description known-issue file line
//   fail      where expr exp info   plan     count
//   skip      reason                begin    id
//   note      text                  fail     id where expression expansion info
//   end       id status ms          skip     id reason
//   teardown                        note     id text
//   done                            crash    id what
//                                   output   id line
//                                   end      id status milliseconds
//                                   summary  pass fail skip crash timeout known milliseconds
//
// A worker runs several tests: fail, skip and note belong to the test between the last begin and its end. teardown
// comes before the last test takes Onyx down, done once every test passed and the worker is about to exit. A tool may
// see begin twice for a test: one that went wrong in a worker it shared runs again alone (supervisor.cpp).

#include "alias.hpp"
#include <cstring>
#include <string>

namespace OnyxTest
{
constexpr const char *ProtocolMarker = "@onyx-test";

namespace Detail
{
void AppendEscaped(String &line, const char *data, usz size);
inline void AppendField(String &line, const char *text)
{
    AppendEscaped(line, text, std::strlen(text));
}
inline void AppendField(String &line, const StringView text)
{
    AppendEscaped(line, text.GetData(), text.GetSize());
}
inline void AppendField(String &line, const String &text)
{
    AppendEscaped(line, text.GetData(), text.GetSize());
}
inline void AppendField(String &line, const std::string &text)
{
    AppendEscaped(line, text.data(), text.size());
}
void WriteLine(const String &line);
} // namespace Detail

// writes one event to stdout as a whole line and flushes it: events from several threads never interleave. the line is
// built with the allocator of the thread that emits it (PushThreadAllocator)
template <typename... Fields> void Emit(const char *kind, const Fields &...fields)
{
    String line{ProtocolMarker};
    line += '\t';
    line += kind;
    ((line += '\t', Detail::AppendField(line, fields)), ...);
    Detail::WriteLine(line);
}

// the fields of an event, the kind first, unescaped. false if the line holds no event. a log line written by another
// thread right before the event, on the same line, is skipped
bool ParseEvent(const String &line, Array<String> &fields);

// a source file as reports name it: relative to the repository and with forward slashes, so CI annotations land on the
// right file. a label, not a path to open
String RelativePath(const char *file);
} // namespace OnyxTest
