// Render views: viewports and image sizes against their target, screen/world conversions for every camera mode,
// layering and zoom. Coordinates on screen are logical pixels, relative to the target's top left corner.

#include "harness.hpp"

using namespace OnyxTest;

static bool waitForSize(Onyx::Window *win, const u32v2 &size)
{
    win->SetSize(size);
    return WaitFor([&] { return win->GetSize() == size; });
}

ONYX_TEST(view, proportional_viewport_follows_resizes,
          .Description = "A proportional viewport keeps its window fractions through a resize")
{
    Onyx::Window *win = OpenTestWindow({.Size = {400, 300}});
    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *rv = win->CreateRenderView<D2>(&cam);
    rv->SetProportionalViewport({.Position = {0.5f, 0.f}, .Size = {0.5f, 1.f}}); // right half
    Frame(2);
    CHECK_THAT(rv->GetViewport().Position, IsNear(f32v2(200.f, 0.f), 0.5f));
    CHECK_THAT(rv->GetViewport().Size, IsNear(f32v2(200.f, 300.f), 0.5f));

    REQUIRE(waitForSize(win, {600, 200}));
    Frame(2);
    CHECK_THAT(rv->GetViewport().Position, IsNear(f32v2(300.f, 0.f), 0.5f));
    CHECK_THAT(rv->GetViewport().Size, IsNear(f32v2(300.f, 200.f), 0.5f));
}

ONYX_TEST(view, fixed_viewport_ignores_resizes,
          .Description = "A fixed viewport keeps its position and size in logical pixels through a resize")
{
    Onyx::Window *win = OpenTestWindow({.Size = {400, 300}});
    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *rv = win->CreateRenderView<D2>(&cam);
    rv->SetFixedViewport({.Position = {10.f, 20.f}, .Size = {100.f, 80.f}});
    REQUIRE(waitForSize(win, {500, 350}));
    Frame(2);
    CHECK_THAT(rv->GetViewport().Position, IsNear(f32v2(10.f, 20.f), 1e-3f));
    CHECK_THAT(rv->GetViewport().Size, IsNear(f32v2(100.f, 80.f), 1e-3f));
}

ONYX_TEST(view, image_size_follows_the_viewport,
          .Description = "With proportional image sizing a view's image has its viewport's logical size, and the "
                         "physical extent is that times the content scale")
{
    Onyx::Window *win = OpenTestWindow({.Size = {400, 300}});
    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *rv = win->CreateRenderView<D2>(&cam);
    rv->SetProportionalViewport({.Position = {0.f, 0.f}, .Size = {0.5f, 0.5f}});
    Frame(2);
    CHECK_THAT(f32v2{rv->GetImageSize()}, IsNear(Math::Round(rv->GetViewport().Size), 1.f));
    CHECK(rv->GetImageExtent() == Onyx::ApplyScale(rv->GetImageSize(), win->GetContentScale()));

    REQUIRE(waitForSize(win, {300, 300}));
    Frame(2);
    CHECK_THAT(f32v2{rv->GetImageSize()}, IsNear(f32v2(150.f, 150.f), 1.f));

    rv->SetFixedImageSize({64, 32});
    REQUIRE(waitForSize(win, {420, 320}));
    Frame(2);
    CHECK(rv->GetImageSize() == u32v2(64, 32));
}

ONYX_TEST(view, screen_world_round_trip_2d,
          .Description = "ScreenToWorld and WorldToScreen are inverses for 2D cameras, moved, rotated and zoomed")
{
    Scene<D2> scene{{200, 120}};
    const f32v2 probes[] = {{0.f, 0.f}, {37.f, 81.f}, {100.f, 60.f}, {199.f, 119.f}};
    for (const Onyx::CameraMode mode : {Onyx::CameraMode_Orthographic, Onyx::CameraMode_Viewport})
        for (const f32 angle : {0.f, 0.7f, -2.1f})
        {
            scene.Camera.Mode = mode;
            scene.Camera.View.Translation = f32v2{3.f, -1.5f};
            scene.Camera.View.Rotation = angle;
            scene.Camera.OrthoParameters.Size = 7.f;
            Frame(1);
            for (const f32v2 &p : probes)
            {
                const f32v2 back = scene.View->WorldToScreen(scene.View->ScreenToWorld(p));
                CHECK_MSG(Math::Approximately(back, p, 1e-2f), "Mode {}, angle {}: {} came back as {}", u32(mode),
                          angle, p, back);
            }
        }
}

ONYX_TEST(view, screen_world_round_trip_3d,
          .Description = "In 3D a screen point and a depth define a world point that projects back onto the same "
                         "screen point, for every camera mode")
{
    Scene<D3> scene{{200, 120}};
    scene.Camera.View.Translation = f32v3{1.f, 2.f, 6.f};
    for (const Onyx::CameraMode mode :
         {Onyx::CameraMode_Perspective, Onyx::CameraMode_Orthographic, Onyx::CameraMode_Viewport})
    {
        scene.Camera.Mode = mode;
        Frame(1);
        for (const f32 depth : {0.1f, 0.5f, 0.9f})
            for (const f32v2 p : {f32v2{10.f, 10.f}, f32v2{100.f, 60.f}, f32v2{190.f, 100.f}})
            {
                const f32v3 world = scene.View->ScreenToWorld(f32v3{p, depth});
                const f32v2 back = scene.View->WorldToScreen(world);
                CHECK_MSG(Math::Approximately(back, p, 5e-2f), "Mode {}, depth {}: {} came back as {}", u32(mode),
                          depth, p, back);
            }
    }
}

ONYX_TEST(view, orthographic_size_spans_the_viewport_height,
          .Description = "An orthographic camera shows OrthoParameters.Size world units from the top to the bottom "
                         "of its viewport, whatever the viewport size")
{
    Scene<D2> scene{{160, 90}};
    scene.Camera.OrthoParameters.Size = 6.f;
    Frame(1);
    const Onyx::Viewport &vp = scene.View->GetViewport();
    const f32v2 top = scene.View->ScreenToWorld(f32v2{0.5f * vp.Size[0], 0.f});
    const f32v2 bottom = scene.View->ScreenToWorld(f32v2{0.5f * vp.Size[0], vp.Size[1]});
    CHECK_THAT(Math::Norm(top - bottom), IsNear(6.f, 1e-3f));

    Scene<D3> scene3{{160, 90}};
    scene3.Camera.Mode = Onyx::CameraMode_Orthographic;
    scene3.Camera.OrthoParameters.Size = 4.f;
    Frame(1);
    const f32v3 top3 = scene3.View->ScreenToWorld(f32v3{80.f, 0.f, 0.5f});
    const f32v3 bottom3 = scene3.View->ScreenToWorld(f32v3{80.f, 90.f, 0.5f});
    CHECK_THAT(Math::Norm(top3 - bottom3), IsNear(4.f, 1e-3f));
}

ONYX_TEST(view, viewport_mode_is_one_unit_per_pixel,
          .Description = "CameraMode_Viewport maps one world unit to one logical pixel, in 2D and in 3D")
{
    Scene<D2> scene{{128, 96}};
    scene.Camera.Mode = Onyx::CameraMode_Viewport;
    Frame(1);
    const f32v2 p{40.f, 30.f};
    const f32v2 dx = scene.View->ScreenToWorld(p + f32v2{1.f, 0.f}) - scene.View->ScreenToWorld(p);
    const f32v2 dy = scene.View->ScreenToWorld(p + f32v2{0.f, 1.f}) - scene.View->ScreenToWorld(p);
    CHECK_THAT(Math::Norm(dx), IsNear(1.f, 1e-3f));
    CHECK_THAT(Math::Norm(dy), IsNear(1.f, 1e-3f));

    Scene<D3> scene3{{128, 96}};
    scene3.Camera.Mode = Onyx::CameraMode_Viewport;
    Frame(1);
    const f32v3 q = scene3.View->ScreenToWorld(f32v3{p, 0.5f});
    CHECK_THAT(Math::Norm(scene3.View->ScreenToWorld(f32v3{p + f32v2{1.f, 0.f}, 0.5f}) - q), IsNear(1.f, 1e-3f));
    CHECK_THAT(Math::Norm(scene3.View->ScreenToWorld(f32v3{p + f32v2{0.f, 1.f}, 0.5f}) - q), IsNear(1.f, 1e-3f));
}

// Fails until fixed: setViewport changes the viewport at once, but the projection only follows when the renderer caches
// it on the next render. In between, ScreenToWorld and WorldToScreen mix the new viewport with the old projection. The
// overlay maps through them, so on the first frame after every resize it lays out and hit tests with distances off by
// the old/new size ratio (a tiling compositor resizes each window right after mapping it; a drag resize does it every
// frame). Calling CacheMatrices at the end of setViewport fixes it
ONYX_TEST(view, conversions_follow_a_resize_before_the_next_render,
          .Description = "As soon as a resize reaches a view, screen/world conversions use its new viewport, without "
                         "waiting for the next render")
{
    Onyx::Window *win = OpenTestWindow({.Size = {400, 300}});
    Onyx::Camera<D2> cam{};
    cam.Mode = Onyx::CameraMode_Viewport;
    Onyx::RenderView<D2> *rv = win->CreateRenderView<D2>(&cam);
    Frame(2);

    win->SetSize({600, 200});
    // WaitFor checks after polling and before rendering: the view got its viewport, nothing rendered since
    REQUIRE(WaitFor([&] { return rv->GetViewport().Size == f32v2(600.f, 200.f); }));
    const f32v2 p{100.f, 50.f};
    const f32v2 dx = rv->ScreenToWorld(p + f32v2{1.f, 0.f}) - rv->ScreenToWorld(p);
    const f32v2 dy = rv->ScreenToWorld(p + f32v2{0.f, 1.f}) - rv->ScreenToWorld(p);
    CHECK_THAT(Math::Norm(dx), IsNear(1.f, 1e-3f));
    CHECK_THAT(Math::Norm(dy), IsNear(1.f, 1e-3f));
}

ONYX_TEST(view, viewport_center_is_the_camera_position,
          .Description = "A 2D camera looks at its own position: the viewport center maps to its translation, even "
                         "rotated")
{
    Scene<D2> scene{{100, 80}};
    scene.Camera.View.Translation = f32v2{-4.f, 2.5f};
    scene.Camera.View.Rotation = 1.2f;
    Frame(1);
    CHECK_THAT(scene.View->ScreenToWorld(f32v2{50.f, 40.f}), IsNear(f32v2(-4.f, 2.5f), 1e-3f));
}

ONYX_TEST(view, perspective_vertical_angle_is_the_field_of_view,
          .Description = "The rays through the top and bottom centers of a perspective viewport meet at the camera "
                         "with the field of view's angle")
{
    Scene<D3> scene{{160, 120}};
    scene.Camera.Mode = Onyx::CameraMode_Perspective;
    scene.Camera.PerspParameters.FieldOfView = Math::Radians(60.f);
    scene.Camera.View.Translation = f32v3{0.f, 0.f, 5.f};
    Frame(1);
    const f32v3 eye = scene.Camera.View.Translation;
    const f32v3 top = scene.View->ScreenToWorld(f32v3{80.f, 0.f, 0.5f});
    const f32v3 bottom = scene.View->ScreenToWorld(f32v3{80.f, 120.f, 0.5f});
    const f32 cosine = Math::Dot(Math::Normalize(top - eye), Math::Normalize(bottom - eye));
    CHECK_THAT(Math::AntiCosine(Math::Clamp(cosine, -1.f, 1.f)), IsNear(Math::Radians(60.f), Math::Radians(0.1f)));
}

ONYX_TEST(view, layers_order_the_views,
          .Description = "GetSortedViews returns views by increasing layer, and BringToTop/BringToBottom move them")
{
    Onyx::RenderTexture *rt = Onyx::CreateRenderTexture({64, 64});
    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *a = rt->CreateRenderView<D2>(&cam);
    Onyx::RenderView<D2> *b = rt->CreateRenderView<D2>(&cam);
    Onyx::RenderView<D2> *c = rt->CreateRenderView<D2>(&cam);

    rt->BringToTop(a);
    auto sorted = rt->GetSortedViews<D2>();
    REQUIRE(sorted.GetSize() == 3u);
    CHECK(sorted[2] == a);

    rt->BringToBottom(c);
    sorted = rt->GetSortedViews<D2>();
    CHECK(sorted[0] == c);
    CHECK(sorted[2] == a);
    for (u32 i = 1; i < sorted.GetSize(); ++i)
        CHECK(sorted[i - 1]->Layer <= sorted[i]->Layer);
    (void)b;
}

ONYX_TEST(view, zoom_keeps_the_point_under_the_cursor,
          .Description = "ZoomScroll on a 2D view zooms around the given screen point: the world point under it stays "
                         "put")
{
    Scene<D2> scene{{200, 150}};
    scene.Camera.View.Translation = f32v2{1.f, 1.f};
    Frame(1);
    const f32v2 cursor{150.f, 40.f};
    const f32v2 before = scene.View->ScreenToWorld(cursor);
    const f32 size = scene.Camera.OrthoParameters.Size;
    scene.View->ZoomScroll(cursor, 0.25f);
    scene.View->CacheMatrices();
    CHECK(scene.Camera.OrthoParameters.Size != size);
    CHECK_THAT(scene.View->ScreenToWorld(cursor), IsNear(before, 1e-3f));
}

ONYX_TEST(view, is_within_viewport, .Description = "IsWithinViewport accepts points inside the viewport only")
{
    Onyx::Window *win = OpenTestWindow({.Size = {400, 300}});
    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *rv = win->CreateRenderView<D2>(&cam);
    rv->SetFixedViewport({.Position = {100.f, 50.f}, .Size = {200.f, 100.f}});
    CHECK(rv->IsWithinViewport({150.f, 100.f}));
    CHECK(!rv->IsWithinViewport({50.f, 100.f}));
    CHECK(!rv->IsWithinViewport({150.f, 200.f}));
}

ONYX_TEST(view, mouse_render_view_is_the_one_under_the_cursor,
          .Description = "GetMouseRenderView returns the topmost view under the mouse, or none outside them")
{
    Onyx::Window *win = OpenTestWindow({.Size = {400, 200}, .Flags = Onyx::WindowFlag_VirtualInput});
    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *left = win->CreateRenderView<D2>(&cam);
    Onyx::RenderView<D2> *right = win->CreateRenderView<D2>(&cam);
    left->SetProportionalViewport({.Position = {0.f, 0.f}, .Size = {0.5f, 1.f}});
    right->SetProportionalViewport({.Position = {0.5f, 0.f}, .Size = {0.5f, 1.f}});
    Frame(2);

    win->InjectMouseMove({100.f, 100.f});
    CHECK(win->GetMouseRenderView<D2>() == left);
    win->InjectMouseMove({300.f, 100.f});
    CHECK(win->GetMouseRenderView<D2>() == right);

    // overlapping: the top layer wins
    left->SetProportionalViewport({.Position = {0.f, 0.f}, .Size = {1.f, 1.f}});
    win->BringToTop(left);
    Frame(1);
    CHECK(win->GetMouseRenderView<D2>() == left);
    win->BringToTop(right);
    CHECK(win->GetMouseRenderView<D2>() == right);

    win->InjectMouseMove({-20.f, 100.f});
    CHECK(win->GetMouseRenderView<D2>() == nullptr);
}

ONYX_TEST(view, window_views_use_the_window_scale,
          .Description = "Views on a window size their framebuffers with the window's content scale, hidden ones too")
{
    Onyx::Window *win = OpenTestWindow();
    Onyx::Camera<D2> cam2{};
    Onyx::Camera<D3> cam3{};
    Onyx::RenderView<D2> *a = win->CreateRenderView<D2>(&cam2);
    Onyx::RenderView<D2> *hidden = win->CreateRenderView<D2>(&cam2, {.Flags = Onyx::RenderViewFlag_Hidden});
    Onyx::RenderView<D3> *b = win->CreateRenderView<D3>(&cam3);
    Frame(2);
    const f32v2 scale = win->GetContentScale();
    CHECK_THAT(a->GetContentScale(), IsNear(scale, 1e-5f));
    CHECK_THAT(hidden->GetContentScale(), IsNear(scale, 1e-5f));
    CHECK_THAT(b->GetContentScale(), IsNear(scale, 1e-5f));
    CHECK(a->GetImageExtent() == Onyx::ApplyScale(a->GetImageSize(), scale));
}

ONYX_TEST(view, scissors_follow_their_viewport,
          .Description = "A proportional scissor is a fraction of the viewport and follows it; a fixed one stays put")
{
    Onyx::Window *win = OpenTestWindow({.Size = {400, 200}});
    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *rv = win->CreateRenderView<D2>(&cam);
    rv->SetProportionalScissor({.Position = {0.25f, 0.f}, .Size = {0.5f, 1.f}});
    Frame(2);
    CHECK_THAT(rv->GetScissor().Position, IsNear(f32v2(100.f, 0.f), 0.5f));
    CHECK_THAT(rv->GetScissor().Size, IsNear(f32v2(200.f, 200.f), 0.5f));

    rv->SetFixedScissor({.Position = {10.f, 20.f}, .Size = {30.f, 40.f}});
    win->SetSize({500, 300});
    REQUIRE(WaitFor([&] { return win->GetSize() == u32v2(500, 300); }));
    Frame(2);
    CHECK_THAT(rv->GetScissor().Position, IsNear(f32v2(10.f, 20.f), 1e-3f));
    CHECK_THAT(rv->GetScissor().Size, IsNear(f32v2(30.f, 40.f), 1e-3f));
}
