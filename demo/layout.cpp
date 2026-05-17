#include "onyx/resources.hpp"
#include "onyx/context.hpp"
#include "onyx/core.hpp"
#include "onyx/onyx.hpp"
#include "tkit/container/stack_array.hpp"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

using Onyx::D2;
using namespace TKit::Alias;

template <typename T> bool Combo(const char *label, T *index, const char *items)
{
    i32 idx = i32(*index);
    if (ImGui::Combo(label, &idx, items))
    {
        *index = T(idx);
        return true;
    }
    return false;
}

struct PanelInfo
{
    Onyx::LayoutPanelParameters PanelParams{.FillColor = Onyx::Color_White,
                                            .Sizing = Onyx::LayoutSizing::Absolute(1.f)};
    Onyx::LayoutTextParameters TextParams{.FontSize = 0.1f};
    Onyx::LayoutElementType Type = Onyx::LayoutElement_Panel;
    std::string Text;
    TKit::TierArray<u32> Children{};
};

void EditPanel(PanelInfo &info, TKit::StackArray<PanelInfo> &panels)
{
    ImGui::PushID(&info);
    Combo("Type", &info.Type, "Panel\0Floating\0Text\0\0");
    if (info.Type == Onyx::LayoutElement_Panel || info.Type == Onyx::LayoutElement_Floating)
    {
        Onyx::LayoutPanelParameters &p = info.PanelParams;
        ImGui::PushID(&p);
        if (info.Type == Onyx::LayoutElement_Floating)
        {
            p.Floating.Enable = true;
            Combo("X Attachment", &p.Floating.Attachment[0], "Left\0Center\0Right\0\0");
            Combo("Y Attachment", &p.Floating.Attachment[1], "Bottom\0Center\0Top\0\0");

            Combo("X Float alignment", &p.Floating.Alignment[0], "Left\0Center\0Right\0\0");
            Combo("Y Float alignment", &p.Floating.Alignment[1], "Bottom\0Center\0Top\0\0");
        }
        else
            p.Floating.Enable = false;

        ImGui::ColorEdit4("Fill color", p.FillColor.GetData());
        ImGui::ColorEdit4("Outline color", p.OutlineColor.GetData());
        ImGui::SliderFloat("Outline width", &p.OutlineWidth, 0.f, 1.f);
        Combo("Layout direction", &p.Direction, "Left to right\0Right to left\0Bottom to top\0Top to bottom\0\0");

        ImGui::Spacing();
        Combo("X Alignment", &p.Alignment[0], "Left\0Center\0Right\0\0");
        Combo("Y Alignment", &p.Alignment[1], "Bottom\0Center\0Top\0\0");

        ImGui::Spacing();
        Combo("X Sizing", &p.Sizing[0].Type, "Absolute\0Normalized\0Fit\0Grow\0\0");
        if (p.Sizing[0].Type == Onyx::LayoutSizing_Absolute || p.Sizing[0].Type == Onyx::LayoutSizing_Normalized)
            ImGui::DragFloat("X Size", &p.Sizing[0].Size, 0.05f, 0.f, TKIT_F32_MAX);
        else
            ImGui::DragFloat2("X Limits", &p.Sizing[0].Min, 0.05f, 0.f, TKIT_F32_MAX);

        Combo("Y Sizing", &p.Sizing[1].Type, "Absolute\0Normalized\0Fit\0Grow\0\0");
        if (p.Sizing[1].Type == Onyx::LayoutSizing_Absolute || p.Sizing[1].Type == Onyx::LayoutSizing_Normalized)
            ImGui::DragFloat("Y Size", &p.Sizing[1].Size, 0.05f, 0.f, TKIT_F32_MAX);
        else
            ImGui::DragFloat2("Y Limits", &p.Sizing[1].Min, 0.05f, 0.f, TKIT_F32_MAX);

        ImGui::DragFloat4("Padding 4", p.Padding.GetData(), 0.01f, 0.f, TKIT_F32_MAX);
        if (ImGui::DragFloat("Padding", p.Padding.GetData(), 0.01f, 0.f, TKIT_F32_MAX))
            p.Padding = p.Padding[0];

        ImGui::DragFloat("Child gap", &p.ChildGap, 0.01f, 0.f, TKIT_F32_MAX);

        Combo("X Child offset type", &p.ChildOffset[0].Type, "Absolute\0Normalized\0Relative\0\0");
        ImGui::DragFloat("X Child offset", &p.ChildOffset[0].Offset, 0.01f);
        Combo("Y Child offset type", &p.ChildOffset[1].Type, "Absolute\0Normalized\0Relative\0\0");
        ImGui::DragFloat("Y Child offset", &p.ChildOffset[1].Offset, 0.01f);

        Combo("X Self offset type", &p.SelfOffset[0].Type, "Absolute\0Normalized\0Relative\0\0");
        ImGui::DragFloat("X Self offset", &p.SelfOffset[0].Offset, 0.01f);
        Combo("Y Self offset type", &p.SelfOffset[1].Type, "Absolute\0Normalized\0Relative\0\0");
        ImGui::DragFloat("Y Self offset", &p.SelfOffset[1].Offset, 0.01f);

        Combo("Overflow", &p.Overflow, "Spill\0Clip\0\0");
        Combo("Shape", &p.Shape.Type, "Circle\0Rectangle\0Rounded rectangle\0\0");
        ImGui::DragFloat("Radius", &p.Shape.Radius, 0.01f, 0.f, TKIT_F32_MAX);
        if (ImGui::Button("Add child"))
        {
            info.Children.Append(panels.GetSize());
            panels.Append();
        }
        if (ImGui::TreeNode("Children"))
        {
            for (const u32 c : info.Children)
            {
                ImGui::Text("----");
                EditPanel(panels[c], panels);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    else
    {
        Onyx::LayoutTextParameters &p = info.TextParams;
        ImGui::PushID(&p);
        ImGui::ColorEdit4("Fill color", p.FillColor.GetData());
        ImGui::ColorEdit4("Outline color", p.OutlineColor.GetData());
        ImGui::SliderFloat("Outline width", &p.OutlineWidth, 0.f, 1.f);
        Combo("Text mode", &p.Mode, "Unbound\0Wrapped\0\0");
        ImGui::DragFloat("Font size", &p.FontSize, 0.01f, 0.f, TKIT_F32_MAX);

        ImGui::InputText("Text", &info.Text);
        ImGui::PopID();
    }
    ImGui::PopID();
}

void DrawPanels(Onyx::Layout &layout, const PanelInfo &info, const TKit::StackArray<PanelInfo> panels)
{
    if (info.Type == Onyx::LayoutElement_Panel || info.Type == Onyx::LayoutElement_Floating)
    {
        layout.BeginPanel(info.PanelParams);
        for (const u32 c : info.Children)
            DrawPanels(layout, panels[c], panels);
        layout.EndPanel();
    }
    else
        layout.Text(info.Text, info.TextParams);
}

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();

    Onyx::Window *win = Onyx::OpenWindow(
        {.Window = {.PresentMode = Onyx::PresentMode_VSync}, .Flags = Onyx::OpenWindowFlag_EnableImGui});

    Onyx::Camera<D2> cam{};
    cam.OrthoParameters.Size = 5.f;
    Onyx::RenderView<D2> *view =
        win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates | Onyx::RenderViewFlag_PostProcess |
                                            Onyx::RenderViewFlag_Outlines);

    ctx->AddTarget(view);

    {
        Onyx::Layout layout{};
        PanelInfo root{};
        root.PanelParams.Sizing = Onyx::LayoutSizing::Absolute(2.f);

        TKit::StackArray<PanelInfo> panels{};
        panels.Reserve(64);
        panels.Append();

        while (Onyx::Running())
        {
            const TKit::Timespan dt = Onyx::GetDeltaTime(win);
            if (!ImGui::GetIO().WantCaptureKeyboard)
                win->ControlCamera(dt, &cam);
            ctx->Flush();
            ctx->RenderFlags(Onyx::RenderModeFlag_Flat | Onyx::RenderModeFlag_Outlined);

            DrawPanels(layout, root, panels);
            layout.Compile();
            ctx->UserInterfaceLayout(layout);

            for (const Onyx::Event &ev : win->GetNewEvents())
                if (!ImGui::GetIO().WantCaptureMouse && ev.Type == Onyx::Event_Scrolled)
                {
                    const f32 factor = win->IsKeyPressed(Onyx::Key_LeftShift) ? 0.05f : 0.005f;
                    view->ZoomScroll(win->GetNormalizedMousePosition(), factor * ev.ScrollOffset[1]);
                }

#ifdef ONYX_ENABLE_IMGUI
            if (CanRenderImGui(win))
            {
                if (ImGui::Begin("Panel editor"))
                {
                    ImGui::Text("Delta time: %.2f ms", dt.AsMilliseconds());
                    EditPanel(root, panels);
                }
                ImGui::End();
            }
#endif

            Onyx::Transfer();
            Onyx::Render();
        }
    }

    Onyx::Terminate();
}
