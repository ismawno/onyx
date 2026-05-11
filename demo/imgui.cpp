#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include <imgui.h>

int main()
{
    Onyx::Initialize();
    Onyx::Window *win = Onyx::OpenWindow(
        {.Window = {.PresentMode = Onyx::PresentMode_VSync}, .Flags = Onyx::OpenWindowFlag_EnableImGui});

    while (Onyx::Running())
    {
        if (Onyx::CanRenderImGui(win))
            ImGui::ShowDemoWindow();

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
