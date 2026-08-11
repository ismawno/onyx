#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/sanitizer_options.hpp"
#include <imgui.h>
int main()
{
    Onyx::Initialize();
    Onyx::Window *win = Onyx::OpenWindow(
        {.Flags = Onyx::OpenWindowFlag_EnableImGui, .ImGuiConfigFlags = ImGuiConfigFlags_DockingEnable});
    while (Onyx::Running())
    {
        if (Onyx::CanRenderImGui(win))
        {
            ImGui::Begin("Host Window");
            ImGui::Text("Hey!");
            ImGui::DockSpace(ImGui::GetID("MyDockSpace"));
            ImGui::End();

            ImGui::ShowDemoWindow();
            ImGui::Begin("Im a test window");
            ImGui::End();
        }
        Onyx::Render();
    }
    Onyx::Terminate();
}
