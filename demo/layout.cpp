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
    Onyx::LayoutPanelParameters PanelParams{.Color = Onyx::Color_White, .Sizing = Onyx::LayoutSizing::Fixed(1.f)};
    Onyx::LayoutTextParameters TextParams{.FontSize = 0.1f};
    Onyx::LayoutElementType Type = Onyx::LayoutElement_Panel;
    std::string Text;
    TKit::TierArray<u32> Children{};
};

void EditPanel(PanelInfo &info, TKit::StackArray<PanelInfo> &panels)
{
    ImGui::PushID(&info);
    Combo("Type", &info.Type, "Panel\0Text\0\0");
    if (info.Type == Onyx::LayoutElement_Panel)
    {
        Onyx::LayoutPanelParameters &p = info.PanelParams;
        ImGui::PushID(&p);
        ImGui::ColorEdit4("Color", p.Color.GetData());
        Combo("Layout direction", &p.Direction, "Horizontal\0Vertical\0\0");

        ImGui::Spacing();
        Combo("X Alignment", &p.Alignment[0], "Left\0Center\0Right\0\0");
        Combo("Y Alignment", &p.Alignment[1], "Bottom\0Center\0Top\0\0");

        ImGui::Spacing();
        Combo("X Sizing", &p.Sizing[0].Type, "Fixed\0Fit\0Grow\0\0");
        if (p.Sizing[0].Type == Onyx::LayoutSizing_Fixed)
            ImGui::DragFloat("X Size", &p.Sizing[0].Size, 0.05f, 0.f, TKIT_F32_MAX);
        else
            ImGui::DragFloat2("X Limits", &p.Sizing[0].Min, 0.05f, 0.f, TKIT_F32_MAX);

        Combo("Y Sizing", &p.Sizing[1].Type, "Fixed\0Fit\0Grow\0\0");
        if (p.Sizing[1].Type == Onyx::LayoutSizing_Fixed)
            ImGui::DragFloat("Y Size", &p.Sizing[1].Size, 0.05f, 0.f, TKIT_F32_MAX);
        else
            ImGui::DragFloat2("Y Limits", &p.Sizing[1].Min, 0.05f, 0.f, TKIT_F32_MAX);

        ImGui::DragFloat4("Padding 4", p.Padding.GetData(), 0.01f, 0.f, TKIT_F32_MAX);
        if (ImGui::DragFloat("Padding", p.Padding.GetData(), 0.01f, 0.f, TKIT_F32_MAX))
            p.Padding = p.Padding[0];

        ImGui::DragFloat("Child gap", &p.ChildGap, 0.01f, 0.f, TKIT_F32_MAX);
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
        ImGui::ColorEdit4("Color", p.Color.GetData());
        Combo("Text mode", &p.Mode, "Unbound\0Wrapped\0\0");
        ImGui::DragFloat("Font size", &p.FontSize, 0.01f, 0.f, TKIT_F32_MAX);

        ImGui::InputText("Text", &info.Text);
        ImGui::PopID();
    }
    ImGui::PopID();
}

void DrawPanels(Onyx::Layout &layout, const PanelInfo &info, const TKit::StackArray<PanelInfo> panels)
{
    if (info.Type == Onyx::LayoutElement_Panel)
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
    const Onyx::StatMeshData<D2> qdata = Onyx::CreateQuadMeshData<D2>();
    const Onyx::ParaMeshData<D2> rqdata = Onyx::CreateRoundedQuadMeshData<D2>();
    const Onyx::FontData fdata = ONYX_CHECK_RESULT(Onyx::LoadDefaultFont());

    const Onyx::ResourcePool spool = Onyx::Resources::CreateResourcePool<D2>(Onyx::Resource_StaticMesh);
    const Onyx::ResourcePool ppool = Onyx::Resources::CreateResourcePool<D2>(Onyx::Resource_ParametricMesh);
    const Onyx::ResourcePool fpool = Onyx::Resources::CreateFontPool();

    const Onyx::Resource quad = Onyx::Resources::RegisterMesh(spool, qdata);
    const Onyx::Resource roundedSquare = Onyx::Resources::RegisterMesh(ppool, rqdata);
    const Onyx::Resource sampler = Onyx::Resources::CreateSampler();
    const Onyx::Resource font = Onyx::Resources::RegisterFont(fpool, fdata);

    Onyx::Resources::Sync(Onyx::SyncFlag_StaticMeshes | Onyx::SyncFlag_ParametricMeshes | Onyx::SyncFlag_Fonts);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();

    Onyx::Window *win = Onyx::OpenWindow(
        {.Window = {.PresentMode = Onyx::PresentMode_VSync}, .Flags = Onyx::OpenWindowFlag_EnableImGui});

    Onyx::Camera<D2> cam{};
    cam.OrthoParameters.Size = 5.f;
    Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);

    ctx->AddTarget(view);

    {
        Onyx::LayoutSpecs spc;
        spc.RectangleMesh = quad;
        spc.RoundedRectangleMesh = roundedSquare;
        spc.Font = font;
        Onyx::Layout layout{spc};
        PanelInfo root{};
        root.PanelParams.Sizing = Onyx::LayoutSizing::Fixed(2.f);

        TKit::StackArray<PanelInfo> panels{};
        panels.Reserve(64);
        panels.Append();

        while (Onyx::Running())
        {
            const TKit::Timespan dt = Onyx::GetDeltaTime(win);
            if (!ImGui::GetIO().WantCaptureKeyboard)
                win->ControlCamera(dt, &cam);
            ctx->Flush();
            ctx->RenderFlags(Onyx::RenderModeFlag_Flat);
            ctx->FontSampler(sampler);

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
