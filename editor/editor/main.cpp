#include "editor.hpp"
#include "onyx/onyx.hpp"
#include "onyx/sanitizer_options.hpp"

int main()
{
    Editor::Initialize();

    while (Onyx::Running())
        Editor::Run();

    Editor::Terminate();
}
