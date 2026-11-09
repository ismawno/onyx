#include "protocol.hpp"
#include "tkit/utils/logging.hpp"
#include <cstdio>
#include <mutex>

namespace OnyxTest
{
namespace Detail
{
void AppendEscaped(TKit::DynamicString &line, const char *data, const usz size)
{
    for (usz i = 0; i < size; ++i)
        switch (const char c = data[i])
        {
        case '\\':
            line += "\\\\";
            break;
        case '\t':
            line += "\\t";
            break;
        case '\n':
            line += "\\n";
            break;
        case '\r':
            line += "\\r";
            break;
        default:
            line += c;
        }
}

static std::mutex s_OutputMutex{};
void WriteLine(const TKit::DynamicString &line)
{
    const std::lock_guard lock{s_OutputMutex};
    TKit::PrintLine("{}", line);
    std::fflush(stdout);
}
} // namespace Detail

static TKit::DynamicString unescape(const char *data, const usz size)
{
    TKit::DynamicString out{};
    for (usz i = 0; i < size; ++i)
    {
        if (data[i] != '\\' || i + 1 == size)
        {
            out += data[i];
            continue;
        }
        switch (data[++i])
        {
        case 't':
            out += '\t';
            break;
        case 'n':
            out += '\n';
            break;
        case 'r':
            out += '\r';
            break;
        default:
            out += data[i];
        }
    }
    return out;
}

bool ParseEvent(const TKit::DynamicString &line, TKit::DynamicArray<TKit::DynamicString> &fields)
{
    fields.Clear();
    const TKit::DynamicString marker = TKit::DynamicString{ProtocolMarker} + "\t";
    const usz at = line.Find(marker.CString());
    if (at == TKit::DynamicString::npos)
        return false;

    const char *data = line.GetData() + at + marker.GetSize();
    const usz size = line.GetSize() - at - marker.GetSize();
    usz start = 0;
    for (usz i = 0; i <= size; ++i)
        if (i == size || data[i] == '\t')
        {
            fields.Append(unescape(data + start, i - start));
            start = i + 1;
        }
    return !fields.IsEmpty();
}

TKit::DynamicString RelativePath(const char *file)
{
    TKit::DynamicString path{file};
    path.Replace('\\', '/');
    // the last one: the checkout itself may sit in a directory called tests
    usz last = TKit::DynamicString::npos;
    for (usz at = path.Find("/tests/"); at != TKit::DynamicString::npos; at = path.Find("/tests/", at + 1))
        last = at;
    return last == TKit::DynamicString::npos ? path : path.SubString(last + 1);
}
} // namespace OnyxTest
