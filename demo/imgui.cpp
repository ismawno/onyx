#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "onyx/sanitizer_options.hpp"
#include <imgui.h>

using Onyx::D2;
using Onyx::D3;

int main()
{
    Onyx::Specs specs{};
    specs.MaxImGuiVertices = 65536;
    specs.MaxImGuiIndices = 98304;
    specs.Specs2 = Onyx::CreateEmptySpecs<D2>();
    specs.Specs3 = Onyx::CreateEmptySpecs<D3>();
    Onyx::Initialize(specs);

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
        Onyx::Transfer();
        Onyx::Render();
    }
    Onyx::Terminate();
}
