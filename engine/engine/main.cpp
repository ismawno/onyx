#include "engine.hpp"
#include "onyx/sanitizer_options.hpp"

int main()
{
    Engine::Initialize();
    Engine::Run();
    Engine::Terminate();
}
