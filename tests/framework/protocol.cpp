#include "protocol.hpp"
#include "tkit/utils/logging.hpp"
#include <cstdio>
#include <mutex>

namespace OnyxTest
{
namespace Detail
{
void AppendEscaped(String &line, const char *data, const usz size)
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
void WriteLine(const String &line)
{
    const std::lock_guard lock{s_OutputMutex};
    TKit::PrintLine("{}", line);
    std::fflush(stdout);
}
} // namespace Detail

static String unescape(const char *data, const usz size)
{
    String out{};
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

bool ParseEvent(const String &line, Array<String> &fields)
{
    fields.Clear();
    const String marker = String{ProtocolMarker} + "\t";
    const usize at = line.Find(marker.CString());
    if (at == String::npos)
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

String RelativePath(const char *file)
{
    String path{file};
    path.Replace('\\', '/');
    // the last one: the checkout itself may sit in a directory called tests
    usize last = String::npos;
    for (usize at = path.Find("/tests/"); at != String::npos; at = path.Find("/tests/", at + 1))
        last = at;
    return last == String::npos ? path : path.SubString(last + 1);
}
} // namespace OnyxTest
