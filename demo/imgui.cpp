#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include <imgui.h>

int main()
{
    Onyx::Initialize();
    Onyx::Window *win = Onyx::OpenWindow({.Flags = Onyx::OpenWindowFlag_EnableImGui});

    Onyx::SetTargetDeltaTime(win, TKit::Timespan::FromSeconds(1.f / 60.f));
    while (Onyx::Running())
    {
        if (Onyx::CanRenderImGui(win))
            ImGui::ShowDemoWindow();

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}
