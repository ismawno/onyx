#include "onyx/application/application.hpp"
#include "tkit/profiling/macros.hpp"
#include "sandbox/sandbox.hpp"
#include "sandbox/argparse.hpp"

using namespace Onyx;

int main(int argc, char **argv)
{
    TKIT_PROFILE_NOOP();
    const ParseData pdata = ParseArgs(argc, argv);
    ONYX_CHECK_EXPRESSION(Initialize());
    {
        Onyx::Application app{};
        app.SetApplicationLayer<SandboxAppLayer>(&pdata);
        ONYX_CHECK_EXPRESSION(app.Run());
    }
    Terminate();
}
