#include "sandbox/sandbox.hpp"
#include "onyx/application/application.hpp"
#include "tkit/profiling/macros.hpp"

using namespace Onyx;

int main()
{
    TKIT_PROFILE_NOOP()
    ONYX_CHECK_EXPRESSION(Core::Initialize());
    {
        Onyx::Application app{};
        app.SetApplicationLayer<SandboxAppLayer>();
        ONYX_CHECK_EXPRESSION(app.Run());
    }
    Core::Terminate();
}
