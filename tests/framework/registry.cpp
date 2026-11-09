#include "test.hpp"

namespace OnyxTest
{
// function local: registrars run during static initialization, in any translation unit order
static TKit::DynamicArray<TestInfo> &registry()
{
    static TKit::DynamicArray<TestInfo> tests{};
    return tests;
}

Registrar::Registrar(const char *suite, const char *name, const char *file, const u32 line, const TestOptions &options)
{
    TestInfo info{};
    info.Suite = suite;
    info.Name = name;
    info.Id = TKit::DynamicString::Format("{}.{}", suite, name);
    info.File = file;
    info.Line = line;
    info.Options = options;
    registry().Append(std::move(info));
}

const TKit::DynamicArray<TestInfo> &GetTests()
{
    return registry();
}
const TestInfo *FindTest(const char *id)
{
    for (const TestInfo &test : registry())
        if (test.Id == id)
            return &test;
    return nullptr;
}
} // namespace OnyxTest
