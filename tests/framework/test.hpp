#pragma once

// Onyx runtime tests: how a test is declared, on top of Catch2 (the version Toolkit and vulkit test with).
//
// ONYX_TEST(suite, name, options...) declares a Catch2 TEST_CASE named "suite.name", tagged [suite], and records what
// the runner needs to run it: how to initialize Onyx for it, its timeout, its description. Inside, Catch2 applies:
// CHECK records a failure and goes on, REQUIRE ends the test, CHECK_THAT takes a matcher (IsNear below, IsColor in
// harness.hpp), INFO and CAPTURE add context to the failures after them, SKIP skips the test.
//
// Catch2 runs the test body, but not the run: every test gets a process of its own (worker.cpp), because tests
// initialize and terminate Onyx, open windows and may crash, panic on a validation error or hang. Catch2 runs a whole
// suite in one process, so one crash would end every test after it, and nothing would end a test that hangs. Onyx
// builds without exceptions, and so does Catch2 here: a failed REQUIRE ends the worker process, which then reports
// the test as failed (worker.cpp).

#include "onyx/alias.hpp"
#include "onyx/core.hpp"
#include "tkit/container/dynamic_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/math/tensor.hpp"
#include "tkit/utils/logging.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include <fmt/format.h>
#include <string>

namespace OnyxTest
{
using namespace TKit::Alias;
namespace Math = TKit::Math;

enum InitMode : u8
{
    // Onyx::Initialize with default specs, plus the default resources of both dimensions (meshes, font, sampler)
    Init_Full,
    // Onyx::Initialize with default specs only
    Init_Bare,
    // nothing: the test initializes and terminates Onyx itself (lifecycle tests, custom specs)
    Init_Manual,
};

struct TestOptions
{
    const char *Description = "";
    InitMode Init = Init_Full;
    // seconds. the worker's watchdog ends a test that runs longer and the runner reports it as timed out
    f32 Timeout = 60.f;
    // a bug the test exposes that you decided to leave for later: a failure, crash or timeout is reported as KNOWN and
    // does not fail the run. once the test passes the runner says so, as a reminder to remove this
    const char *KnownIssue = nullptr;
};

struct TestInfo
{
    const char *Suite;
    const char *Name;
    // "suite.name", the name of its Catch2 test case
    TKit::DynamicString Id;
    const char *File;
    u32 Line;
    TestOptions Options;
};

// every test, in registration order (registry.cpp)
const TKit::DynamicArray<TestInfo> &GetTests();
const TestInfo *FindTest(const char *id);

struct Registrar
{
    Registrar(const char *suite, const char *name, const char *file, u32 line, const TestOptions &options);
};

enum Status : u8
{
    Status_Pass,
    Status_Fail,
    Status_Skip,
    Status_Crash,
    Status_Timeout,
    Status_Known,
    Status_Count,
};
constexpr const char *StatusNames[Status_Count] = {"pass", "fail", "skip", "crash", "timeout", "known"};

// makes a crash end its process instead of waiting on a message box nobody will close (Windows only, crash_dialogs.cpp)
void SilenceCrashDialogs();
// removes the temporary directory the test wrote its files to, if it made one (harness.cpp, TempPath)
void RemoveTempFiles();

// CHECK_THAT(value, IsNear(expected, epsilon)): numbers within epsilon, vectors and matrices component by component.
// the expected value is converted to the type of the checked one, so IsNear(f32v2{1.f, 2.f}, ...) fits any vector of
// two
template <typename T, typename E> class NearMatcher final : public Catch::Matchers::MatcherGenericBase
{
  public:
    NearMatcher(const T &expected, const E epsilon) : m_Expected(expected), m_Epsilon(epsilon)
    {
    }

    template <typename U> bool match(const U &actual) const
    {
        return Math::Approximately(actual, U(m_Expected), m_Epsilon);
    }
    std::string describe() const override
    {
        return fmt::format("is within {} of {}", m_Epsilon, m_Expected);
    }

  private:
    T m_Expected;
    E m_Epsilon;
};
template <typename T, typename E> NearMatcher<T, E> IsNear(const T &expected, const E epsilon)
{
    return NearMatcher<T, E>{expected, epsilon};
}
} // namespace OnyxTest

// how Catch2 prints what it checked: Toolkit's vectors and matrices as Toolkit formats them, Toolkit strings quoted,
// Onyx error codes by name. tensors and strings are ranges, which Catch2 would print element by element: they opt out
namespace Catch
{
template <typename T, TKit::Alias::usize N0, TKit::Alias::usize... N> struct is_range<TKit::Math::Tensor<T, N0, N...>>
{
    static constexpr bool value = false;
};
template <typename AllocState> struct is_range<TKit::Array<char, AllocState>>
{
    static constexpr bool value = false;
};
template <typename T, TKit::Alias::usize N0, TKit::Alias::usize... N>
struct StringMaker<TKit::Math::Tensor<T, N0, N...>>
{
    static std::string convert(const TKit::Math::Tensor<T, N0, N...> &tensor)
    {
        return fmt::format("{}", tensor);
    }
};
template <typename AllocState> struct StringMaker<TKit::Array<char, AllocState>>
{
    static std::string convert(const TKit::Array<char, AllocState> &text)
    {
        return fmt::format("\"{}\"", text.IsEmpty() ? "" : text.CString());
    }
};
template <> struct StringMaker<Onyx::ErrorCode>
{
    static std::string convert(const Onyx::ErrorCode code)
    {
        return Onyx::ToString(code);
    }
};
} // namespace Catch

#define ONYX_TEST_CONCAT_IMPL(a, b) a##b
#define ONYX_TEST_CONCAT(a, b) ONYX_TEST_CONCAT_IMPL(a, b)

// ONYX_TEST(suite, name) or ONYX_TEST(suite, name, .Description = "...", .Init = OnyxTest::Init_Manual)
#define ONYX_TEST(suite, name, ...)                                                                                    \
    static const OnyxTest::Registrar ONYX_TEST_CONCAT(                                                                 \
        onyxTestRegistrar_, ONYX_TEST_CONCAT(suite, ONYX_TEST_CONCAT(_, name))){#suite, #name, __FILE__, __LINE__,     \
                                                                                OnyxTest::TestOptions{__VA_ARGS__}};   \
    TEST_CASE(#suite "." #name, "[" #suite "]")

// a check that says why it failed. Catch2 has no such macro, and an INFO would tag every later failure in its scope:
// UNSCOPED_INFO goes with the next assertion only. the condition is not decomposed (it may hold && or ||, which Catch2
// refuses to decompose): the message says what matters
#define CHECK_MSG(condition, ...)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        UNSCOPED_INFO(TKit::Format(__VA_ARGS__));                                                                      \
        CHECK((condition));                                                                                            \
    } while (false)
#define REQUIRE_MSG(condition, ...)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        UNSCOPED_INFO(TKit::Format(__VA_ARGS__));                                                                      \
        REQUIRE((condition));                                                                                          \
    } while (false)
