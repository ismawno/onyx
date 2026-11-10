#include "test.hpp"
#include <cstring>

namespace OnyxTest
{
// constant initialized, so they are set before any registrar runs, whatever the order of translation units
static const TestInfo *s_First = nullptr;
static TestInfo *s_Last = nullptr;

Registrar::Registrar(const char *suite, const char *name, const char *id, const char *file, const u32 line,
                     const TestOptions &options)
    : Info{suite, name, id, file, line, options}
{
    if (s_Last)
        s_Last->Next = &Info;
    else
        s_First = &Info;
    s_Last = &Info;
}

const TestInfo *GetTests()
{
    return s_First;
}
const TestInfo *FindTest(const char *id)
{
    for (const TestInfo *test = s_First; test; test = test->Next)
        if (std::strcmp(test->Id, id) == 0)
            return test;
    return nullptr;
}
} // namespace OnyxTest
