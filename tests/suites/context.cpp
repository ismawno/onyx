// Render contexts on the CPU side: their state machine (push, pop, flush, transforms), recorded lights and targets.
// What they draw is checked in the render suite.

#include "harness.hpp"

using namespace OnyxTest;

static bool sameColor(const Onyx::Color &a, const Onyx::Color &b)
{
    return Math::Approximately(a.rgba, b.rgba, 1e-5f);
}

ONYX_TEST(context, pop_restores_what_push_saved,
          .Description = "Push saves the whole state and Pop brings it back, nested pairs included")
{
    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->Flush();
    ctx->FillColor(Onyx::Color_Red);
    ctx->Translate(f32v2{1.f, 2.f});
    const Onyx::ContextState<D2> saved = ctx->GetState();

    ctx->Push();
    ctx->FillColor(Onyx::Color_Blue);
    ctx->Scale(3.f);
    ctx->Push();
    ctx->Rotate(0.5f);
    ctx->Alpha(0.5f);
    ctx->Pop();
    ctx->Pop();

    CHECK(sameColor(ctx->GetState().FillColor, saved.FillColor));
    CHECK_THAT(ctx->GetState().Transform, IsNear(saved.Transform, 1e-6f));
    CHECK(ctx->GetState().Blend == saved.Blend);
}

ONYX_TEST(context, flush_resets_the_state,
          .Description = "Flush brings back the default state: identity transform, white fill, flat and opaque")
{
    Onyx::RenderContext<D3> *ctx = Onyx::CreateRenderContext<D3>();
    ctx->Flush();
    ctx->FillColor(Onyx::Color_Green);
    ctx->Translate(f32v3{1.f, 0.f, -3.f});
    ctx->Alpha(0.3f);
    ctx->RenderFlags(Onyx::RenderModeFlag_Shaded);
    ctx->Flush();

    const Onyx::ContextState<D3> &state = ctx->GetState();
    CHECK_THAT(state.Transform, IsNear(f32m4::Identity(), 1e-6f));
    CHECK(sameColor(state.FillColor, Onyx::Color_White));
    CHECK(state.RenderFlags == Onyx::RenderModeFlag_Flat);
    CHECK(state.Blend == Onyx::BlendPass_Opaque);
    CHECK(state.Font == Onyx::Resources_GetDefault().Font);
}

ONYX_TEST(context, extrinsic_and_intrinsic_transforms_differ,
          .Description = "Extrinsic transforms apply in world axes (after what was there), intrinsic ones in local "
                         "axes (before it)")
{
    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->Flush();
    ctx->Scale(2.f);
    ctx->Translate(f32v2{1.f, 0.f}); // extrinsic: the translation is not scaled
    CHECK_THAT(ctx->LocalToWorld(f32v2{0.f}), IsNear(f32v2(1.f, 0.f), 1e-5f));

    ctx->Flush();
    ctx->Scale(2.f);
    ctx->Translate(f32v2{1.f, 0.f}, Onyx::Transform_Intrinsic); // intrinsic: moves along the scaled local axis
    CHECK_THAT(ctx->LocalToWorld(f32v2{0.f}), IsNear(f32v2(2.f, 0.f), 1e-5f));
}

ONYX_TEST(context, local_and_world_are_inverses,
          .Description = "WorldToLocal undoes LocalToWorld for any accumulated transform")
{
    Onyx::RenderContext<D3> *ctx = Onyx::CreateRenderContext<D3>();
    ctx->Flush();
    ctx->Translate(f32v3{1.f, -2.f, 3.f});
    ctx->RotateY(0.7f);
    ctx->Scale(f32v3{2.f, 0.5f, 1.5f});
    ctx->RotateX(-1.1f);
    for (const f32v3 p : {f32v3{0.f}, f32v3{1.f, 2.f, 3.f}, f32v3{-4.f, 0.5f, 7.f}})
        CHECK_THAT(ctx->WorldToLocal(ctx->LocalToWorld(p)), IsNear(p, 1e-4f));
}

ONYX_TEST(context, lights_are_recorded,
          .Description = "Lights are recorded with their parameters (shadow filter flags included) and cleared by "
                         "Flush")
{
    Onyx::RenderContext<D2> *ctx2 = Onyx::CreateRenderContext<D2>();
    ctx2->Flush();
    ctx2->PointLight(
        {.Position = f32v2{1.f, 2.f}, .Intensity = 0.5f, .Flags = Onyx::LightFlag_CastShadows | Onyx::LightFlag_PCF});
    ctx2->DirectionalLight({.Angle = 0.25f, .Flags = Onyx::LightFlag_CastShadows | Onyx::LightFlag_PCSS});
    REQUIRE(ctx2->GetPointLightParams().GetSize() == 1u);
    REQUIRE(ctx2->GetDirectionalLightParams().GetSize() == 1u);
    CHECK_THAT(ctx2->GetPointLightParams()[0].Position, IsNear(f32v2(1.f, 2.f), 1e-5f));
    CHECK_THAT(ctx2->GetPointLightParams()[0].Intensity, IsNear(0.5f, 1e-6f));
    CHECK_THAT(ctx2->GetDirectionalLightParams()[0].Angle, IsNear(0.25f, 1e-5f));
    // the shadow filter travels with the light
    CHECK(ctx2->GetPointLightParams()[0].Flags == (Onyx::LightFlag_CastShadows | Onyx::LightFlag_PCF));
    CHECK(ctx2->GetDirectionalLightParams()[0].Flags == (Onyx::LightFlag_CastShadows | Onyx::LightFlag_PCSS));

    Onyx::RenderContext<D3> *ctx3 = Onyx::CreateRenderContext<D3>();
    ctx3->Flush();
    ctx3->PointLight({.Position = f32v3{0.f, 1.f, 0.f}});
    ctx3->SpotLight({.Position = f32v3{2.f, 0.f, 0.f}, .Direction = f32v3{0.f, -1.f, 0.f}});
    REQUIRE(ctx3->GetPointLightParams().GetSize() == 1u);
    REQUIRE(ctx3->GetSpotLightParams().GetSize() == 1u);
    CHECK_THAT(ctx3->GetSpotLightParams()[0].Position, IsNear(f32v3(2.f, 0.f, 0.f), 1e-5f));
    CHECK_THAT(Math::Normalize(ctx3->GetSpotLightParams()[0].Direction), IsNear(f32v3(0.f, -1.f, 0.f), 1e-5f));

    ctx3->Flush();
    CHECK(ctx3->GetPointLightParams().IsEmpty());
    CHECK(ctx3->GetSpotLightParams().IsEmpty());
}

ONYX_TEST(context, ambient_light_reads_back, .Description = "SetAmbientLight and SetAmbientIntensity read back")
{
    Onyx::RenderContext<D3> *ctx = Onyx::CreateRenderContext<D3>();
    ctx->Flush();
    ctx->SetAmbientLight(Onyx::Color{0.2f, 0.3f, 0.4f, 0.5f});
    CHECK(sameColor(ctx->GetAmbientLight(), Onyx::Color{0.2f, 0.3f, 0.4f, 0.5f}));
    ctx->SetAmbientIntensity(0.9f);
    CHECK_THAT(ctx->GetAmbientLight().rgba[3], IsNear(0.9f, 1e-6f));
}

ONYX_TEST(context, targets_are_added_and_removed,
          .Description = "A context targets exactly the views it was pointed at")
{
    Onyx::RenderTexture *rt = Onyx::CreateRenderTexture({32, 32});
    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *a = rt->CreateRenderView<D2>(&cam);
    Onyx::RenderView<D2> *b = rt->CreateRenderView<D2>(&cam);
    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    CHECK(!ctx->HasTarget(a));
    ctx->AddTarget(a);
    CHECK(ctx->HasTarget(a));
    CHECK(!ctx->HasTarget(b));
    ctx->AddTarget(b);
    CHECK(ctx->HasAllTargets(a->GetViewBit() | b->GetViewBit()));
    ctx->RemoveTarget(a);
    CHECK(!ctx->HasTarget(a));
    CHECK(ctx->HasAnyTarget(a->GetViewBit() | b->GetViewBit()));
    CHECK(a->GetViewBit() != b->GetViewBit());
}

ONYX_TEST(context, alpha_selects_the_transparent_pass,
          .Description = "A translucent fill goes to the transparent pass and an opaque one back to the opaque pass")
{
    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->Flush();
    ctx->Alpha(0.5f);
    CHECK(ctx->GetState().Blend == Onyx::BlendPass_Transparent);
    CHECK_THAT(ctx->GetState().FillColor.rgba[3], IsNear(0.5f, 1e-6f));
    ctx->Alpha(1.f);
    CHECK(ctx->GetState().Blend == Onyx::BlendPass_Opaque);
}
