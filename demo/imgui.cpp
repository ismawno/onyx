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
        {
            ImGui::ShowDemoWindow();
            ImGui::Begin("Im a test window");
            ImGui::Text("Some text %f", Onyx::GetDeltaTime(win).AsMilliseconds());
            ImGui::SetItemTooltip("Hey!");
            ImGui::ShowStyleEditor();
            ImGui::End();
        }
        Onyx::Render();
    }
    Onyx::Terminate();
}
