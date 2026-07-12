#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include <imgui.h>

int main()
{
    Onyx::Initialize();
    Onyx::Window *win = Onyx::OpenWindow({.Flags = Onyx::OpenWindowFlag_EnableImGui});

    while (Onyx::Running())
    {
        if (Onyx::CanRenderImGui(win))
        {
            ImGui::ShowDemoWindow();
            ImGui::Begin("Im a test window");
            ImGui::End();
        }
        Onyx::Render();
    }
    Onyx::Terminate();
}
