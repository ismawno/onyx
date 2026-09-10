#include "engine.hpp"
#include "onyx/sanitizer_options.hpp"

int main()
{
    if (Engine::Initialize())
    {
        Engine::Run();
        Engine::Terminate();
    }
}
