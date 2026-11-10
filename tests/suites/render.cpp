// What ends up on screen: scenes are rendered into render textures, read back and probed pixel by pixel. No window is
// needed, so these run the same on a desktop and on a headless runner. Probes avoid edges: rasterization rules and
// anti aliasing may differ by a pixel between drivers, the colors inside a shape may not.

#include "harness.hpp"
#include "onyx/image.hpp"

using namespace OnyxTest;

static const u8v4 Black{0, 0, 0, 255};
static const u8v4 Red = ToBytes(Onyx::Color_Red);
static const u8v4 Green = ToBytes(Onyx::Color_Green);
static const u8v4 Blue = ToBytes(Onyx::Color_Blue);

// a pixel of a capture, by normalized position: (0, 0) is the top left corner. a failure says which pixel and prints
// its color next to the one expected
#define CHECK_PIXEL(img, x, y, expected, tolerance)                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        UNSCOPED_INFO(TKit::Format("pixel at ({}, {})", x, y));                                                        \
        CHECK_THAT((img).AtNormalized(x, y), IsColor(expected, tolerance));                                            \
    } while (false)

// pixels that are neither of two colors: blended edges
static u32 countBetween(const Capture &img, const u8v4 &a, const u8v4 &b)
{
    u32 count = 0;
    for (u32 y = 0; y < img.Size[1]; ++y)
        for (u32 x = 0; x < img.Size[0]; ++x)
        {
            const u8v4 p = img.At(x, y);
            count += !Near(p, a, 8) && !Near(p, b, 8);
        }
    return count;
}

ONYX_TEST(render, clear_color_fills_the_target, .Description = "An empty view shows its clear color everywhere")
{
    Scene<D2> scene{};
    scene.View->ClearColor = Onyx::Color{0.f, 0.5f, 1.f};
    const Capture img = Render(scene.Target);
    REQUIRE(!img.IsEmpty());
    const u8v4 expected = ToBytes(scene.View->ClearColor);
    const u32 matching = img.Count(expected);
    CHECK_MSG(matching == img.Size[0] * img.Size[1], "{} of {} pixels are {}, the first one is {}", matching,
              img.Size[0] * img.Size[1], expected, img.At(0, 0));
}

ONYX_TEST(render, quad_covers_its_area,
          .Description = "A unit quad in a view two units tall covers the middle half and leaves the rest clear")
{
    Scene<D2> scene{};
    scene.Camera.OrthoParameters.Size = 2.f; // the view spans [-1, 1] vertically
    scene.Context->FillColor(Onyx::Color_Red);
    scene.Context->Quad(); // [-0.5, 0.5]

    const Capture img = Render(scene.Target);
    REQUIRE(!img.IsEmpty());
    CHECK_PIXEL(img, 0.5f, 0.5f, Red, 3);
    CHECK_PIXEL(img, 0.3f, 0.7f, Red, 3);
    for (const f32v2 corner : {f32v2{0.05f, 0.05f}, f32v2{0.95f, 0.05f}, f32v2{0.05f, 0.95f}, f32v2{0.95f, 0.95f}})
        CHECK_PIXEL(img, corner[0], corner[1], Black, 3);

    const u32 total = img.Size[0] * img.Size[1];
    const u32 covered = img.Count(Red, 8);
    CHECK_MSG(covered > total / 4 - img.Size[0] && covered < total / 4 + img.Size[0], "{} of {} pixels are red",
              covered, total);
}

ONYX_TEST(render, circle_is_round,
          .Description = "A circle fills its center and the middle of its sides but not the corners of its square")
{
    Scene<D2> scene{{96, 96}};
    scene.Camera.OrthoParameters.Size = 1.2f;
    scene.Context->FillColor(Onyx::Color_Green);
    scene.Context->Circle(); // diameter 1
    const Capture img = Render(scene.Target);
    REQUIRE(!img.IsEmpty());
    CHECK_PIXEL(img, 0.5f, 0.5f, Green, 3);
    // 0.45 units from the center along an axis: inside
    CHECK_PIXEL(img, 0.5f + 0.45f / 1.2f, 0.5f, Green, 3);
    CHECK_PIXEL(img, 0.5f, 0.5f - 0.45f / 1.2f, Green, 3);
    // the corner of its square, 0.45 units along both axes (0.64 from the center): outside
    CHECK_PIXEL(img, 0.5f + 0.45f / 1.2f, 0.5f + 0.45f / 1.2f, Black, 3);
}

ONYX_TEST(render, transforms_place_shapes,
          .Description = "Translate and Scale move and size a shape in world units, with +y pointing up on screen")
{
    Scene<D2> scene{};
    scene.Camera.OrthoParameters.Size = 4.f; // [-2, 2]
    scene.Context->FillColor(Onyx::Color_Blue);
    scene.Context->Push();
    scene.Context->Scale(0.5f);
    scene.Context->Translate(f32v2{1.f, 1.f}); // upper right quadrant, centered at (1, 1)
    scene.Context->Quad();
    scene.Context->Pop();
    const Capture img = Render(scene.Target);
    REQUIRE(!img.IsEmpty());
    // world (1, 1) is at 3/4 across and 1/4 down
    CHECK_PIXEL(img, 0.75f, 0.25f, Blue, 3);
    CHECK_PIXEL(img, 0.25f, 0.75f, Black, 3);
    CHECK_PIXEL(img, 0.5f, 0.5f, Black, 3);
}

ONYX_TEST(render, camera_moves_the_scene,
          .Description = "Moving the camera right moves what it sees left, and zooming out makes it smaller")
{
    Scene<D2> scene{};
    scene.Camera.OrthoParameters.Size = 4.f;
    scene.Context->FillColor(Onyx::Color_Red);
    scene.Context->Quad();
    const u32 near = Render(scene.Target).Count(Red, 8);

    scene.Camera.View.Translation = f32v2{1.f, 0.f};
    const Capture moved = Render(scene.Target);
    // the quad (at the origin) now sits a quarter of the view left of the center
    CHECK_PIXEL(moved, 0.25f, 0.5f, Red, 3);
    CHECK_PIXEL(moved, 0.5f, 0.5f, Black, 3);

    scene.Camera.View.Translation = f32v2{0.f};
    scene.Camera.OrthoParameters.Size = 8.f;
    const u32 far = Render(scene.Target).Count(Red, 8);
    CHECK_MSG(far * 3 < near && far * 5 > near, "Zooming out by 2 should cover a quarter: {} then {} pixels", near,
              far);
}

ONYX_TEST(render, viewport_limits_drawing,
          .Description = "A view only draws inside its viewport. The rest of the target keeps the target's clear "
                         "color")
{
    Scene<D2> scene{{80, 40}};
    scene.Target->ClearColor = Onyx::Color_Green;
    scene.View->SetProportionalViewport({.Position = {0.f, 0.f}, .Size = {0.5f, 1.f}});
    scene.Context->FillColor(Onyx::Color_Red);
    scene.Context->Scale(100.f); // covers the whole view
    scene.Context->Quad();
    const Capture img = Render(scene.Target);
    REQUIRE(!img.IsEmpty());
    CHECK_PIXEL(img, 0.25f, 0.5f, Red, 3);
    CHECK_PIXEL(img, 0.75f, 0.5f, Green, 3);
}

ONYX_TEST(render, views_composite_by_layer,
          .Description = "Overlapping views are composited by layer: the top one covers the others")
{
    Onyx::RenderTexture *rt = TestRenderTexture({32, 32});
    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *a = rt->CreateRenderView<D2>(&cam);
    Onyx::RenderView<D2> *b = rt->CreateRenderView<D2>(&cam);
    a->ClearColor = Onyx::Color_Red;
    b->ClearColor = Onyx::Color_Blue;

    rt->BringToTop(a);
    CHECK_PIXEL(Render(rt), 0.5f, 0.5f, Red, 3);
    rt->BringToTop(b);
    CHECK_PIXEL(Render(rt), 0.5f, 0.5f, Blue, 3);
}

ONYX_TEST(render, hidden_views_draw_nothing,
          .Description = "RenderViewFlag_Hidden keeps a view out of the composition until the flag is removed")
{
    Scene<D2> scene{};
    scene.Target->ClearColor = Onyx::Color_Green;
    scene.View->ClearColor = Onyx::Color_Red;
    scene.View->AddFlags(Onyx::RenderViewFlag_Hidden);
    CHECK_PIXEL(Render(scene.Target), 0.5f, 0.5f, Green, 3);
    scene.View->RemoveFlags(Onyx::RenderViewFlag_Hidden);
    CHECK_PIXEL(Render(scene.Target, 3), 0.5f, 0.5f, Red, 3);
}

ONYX_TEST(render, contexts_share_views,
          .Description = "Two contexts targeting the same view both show, and removing a target hides its shapes")
{
    Scene<D2> scene{};
    scene.Camera.OrthoParameters.Size = 4.f;
    scene.Context->FillColor(Onyx::Color_Red);
    scene.Context->Translate(f32v2{-1.f, 0.f});
    scene.Context->Quad();

    Onyx::RenderContext<D2> *other = TestContext<D2>();
    other->Flush();
    other->AddTarget(scene.View);
    other->FillColor(Onyx::Color_Blue);
    other->Translate(f32v2{1.f, 0.f});
    other->Quad();

    const Capture both = Render(scene.Target);
    CHECK_PIXEL(both, 0.25f, 0.5f, Red, 3);
    CHECK_PIXEL(both, 0.75f, 0.5f, Blue, 3);

    other->RemoveTarget(scene.View);
    const Capture one = Render(scene.Target, 3);
    CHECK_PIXEL(one, 0.25f, 0.5f, Red, 3);
    CHECK_PIXEL(one, 0.75f, 0.5f, Black, 3);
}

ONYX_TEST(render, static_scenes_persist_until_flushed,
          .Description = "A context that is not flushed keeps showing what it recorded, frame after frame. A flush "
                         "with nothing drawn empties it")
{
    Scene<D2> scene{};
    scene.Camera.OrthoParameters.Size = 2.f;
    scene.Context->FillColor(Onyx::Color_Red);
    scene.Context->Quad();
    CHECK_PIXEL(Render(scene.Target), 0.5f, 0.5f, Red, 3);
    CHECK_PIXEL(Render(scene.Target, 10), 0.5f, 0.5f, Red, 3);

    scene.Context->Flush();
    CHECK_PIXEL(Render(scene.Target, 3), 0.5f, 0.5f, Black, 3);
}

ONYX_TEST(render, transparency_is_order_independent,
          .Description = "With RenderViewFlag_Transparency, overlapping translucent shapes blend to the same color "
                         "whatever order they are drawn in")
{
    const auto draw = [](const bool redFirst) {
        Scene<D2> scene{{32, 32}, {.Flags = Onyx::RenderViewFlag_Transparency}};
        scene.Camera.OrthoParameters.Size = 2.f;
        for (u32 i = 0; i < 2; ++i)
        {
            const bool red = (i == 0) == redFirst;
            scene.Context->FillColor(red ? Onyx::Color_Red : Onyx::Color_Blue);
            scene.Context->Alpha(0.5f);
            scene.Context->Quad();
        }
        return Render(scene.Target).AtNormalized(0.5f, 0.5f);
    };
    const u8v4 a = draw(true);
    const u8v4 b = draw(false);
    CHECK_MSG(Near(a, b, 4), "Red then blue gives {}, blue then red gives {}", a, b);
    // both colors are in there
    CHECK_MSG(a[0] > 40 && a[2] > 40 && a[1] < 20, "Blend is {}", a);
}

ONYX_TEST(render, translucent_fills_blend_with_the_background,
          .Description = "Half transparent white over black lands strictly between them")
{
    Scene<D2> scene{{32, 32}, {.Flags = Onyx::RenderViewFlag_Transparency}};
    scene.Camera.OrthoParameters.Size = 2.f;
    scene.Context->Alpha(0.5f);
    scene.Context->Quad();
    const u8v4 p = Render(scene.Target).AtNormalized(0.5f, 0.5f);
    CHECK_MSG(p[0] > 80 && p[0] < 220 && p[0] == p[1] && p[1] == p[2], "Half white over black is {}", p);
}

ONYX_TEST(render, depth_hides_what_is_behind, .Description = "In 3D the nearest surface wins, whatever the draw order")
{
    Scene<D3> scene{};
    scene.Camera.View.Translation = f32v3{0.f, 0.f, 5.f};
    // near red box first, far (and larger) blue box second
    scene.Context->FillColor(Onyx::Color_Red);
    scene.Context->Push();
    scene.Context->Translate(f32v3{0.f, 0.f, 1.f});
    scene.Context->Box();
    scene.Context->Pop();
    scene.Context->FillColor(Onyx::Color_Blue);
    scene.Context->Push();
    scene.Context->Translate(f32v3{0.f, 0.f, -1.f});
    scene.Context->Scale(2.f);
    scene.Context->Box();
    scene.Context->Pop();

    const Capture img = Render(scene.Target);
    REQUIRE(!img.IsEmpty());
    CHECK_PIXEL(img, 0.5f, 0.5f, Red, 3);
    // the blue box shows around the red one
    CHECK_MSG(img.Count(Blue, 8) > 0, "The far box is not visible at all");
}

ONYX_TEST(render, directional_light_shades_3d_shapes,
          .Description = "A shaded sphere lit from the right is brighter on its right side than on its left")
{
    const Onyx::Resource material = Onyx::Material_Register<D3>();
    Onyx::Resources_Sync(Onyx::SyncFlag_Materials);

    Scene<D3> scene{{64, 64}};
    // the unit sphere then spans about a third of the view
    scene.Camera.View.Translation = f32v3{0.f, 0.f, 2.f};
    scene.Context->Material(material);
    scene.Context->RenderFlags(Onyx::RenderModeFlag_Shaded);
    scene.Context->SetAmbientIntensity(0.05f);
    scene.Context->DirectionalLight({.Direction = f32v3{-1.f, 0.f, 0.f}, .Intensity = 1.f});
    scene.Context->Sphere();

    const Capture img = Render(scene.Target);
    REQUIRE(!img.IsEmpty());
    const u8v4 right = img.AtNormalized(0.58f, 0.5f);
    const u8v4 left = img.AtNormalized(0.42f, 0.5f);
    CHECK_MSG(i32(right[0]) > i32(left[0]) + 20, "Right side {}, left side {}", right, left);
}

ONYX_TEST(render, outlines_surround_shapes,
          .Description = "With RenderViewFlag_Outlines an outlined shape gets a band of its outline color around it")
{
    Scene<D2> scene{{64, 64}, {.Flags = Onyx::RenderViewFlag_Outlines}};
    scene.Camera.OrthoParameters.Size = 2.f;
    scene.Context->FillColor(Onyx::Color_Red);
    scene.Context->OutlineColor(Onyx::Color_Green);
    scene.Context->OutlineWidth(0.2f);
    scene.Context->RenderFlags(Onyx::RenderModeFlag_Flat | Onyx::RenderModeFlag_Outlined);
    scene.Context->Quad();
    const Capture img = Render(scene.Target);
    REQUIRE(!img.IsEmpty());
    CHECK_PIXEL(img, 0.5f, 0.5f, Red, 3);
    CHECK_MSG(img.Count(Green, 8) > 0, "No outline pixel at all");
    CHECK_PIXEL(img, 0.02f, 0.02f, Black, 3);
}

#ifdef ONYX_ENABLE_FONT_LOAD
ONYX_TEST(render, text_draws_glyphs,
          .Description = "Text with the default font puts ink where the string is and nowhere else")
{
    Scene<D2> scene{{128, 64}};
    scene.Camera.OrthoParameters.Size = 2.f;
    scene.Context->FillColor(Onyx::Color_White);
    scene.Context->Scale(0.6f);
    scene.Context->Align(Onyx::Alignment_Center);
    scene.Context->Text("Onyx");
    const Capture img = Render(scene.Target);
    REQUIRE(!img.IsEmpty());
    const u32 ink = img.Size[0] * img.Size[1] - img.Count(Black, 24);
    CHECK_MSG(ink > 40, "Only {} pixels of ink", ink);
    // nothing in the far corners
    CHECK_PIXEL(img, 0.02f, 0.05f, Black, 24);
    CHECK_PIXEL(img, 0.98f, 0.95f, Black, 24);
}
#endif

ONYX_TEST(render, textures_are_sampled,
          .Description = "A quad drawn with a texture shows the texture's pixels, the right way up")
{
    u8 pixels[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    Onyx::ImageData data{};
    data.Data = reinterpret_cast<std::byte *>(pixels);
    data.Width = 2;
    data.Height = 2;
    data.Components = 4;
    data.Format = Graph::Format_R8G8B8A8_SRGB;
    const Onyx::Resource texture = Onyx::Texture_Create(Onyx::Image_Create(data));
    const Onyx::Resource sampler =
        Onyx::Sampler_Create({.MagFilter = Graph::Filter_Nearest, .MinFilter = Graph::Filter_Nearest});
    Onyx::Resources_Sync(Onyx::SyncFlag_All);

    Scene<D2> scene{{64, 64}};
    scene.Camera.OrthoParameters.Size = 1.f; // the unit quad fills the view
    scene.Context->Sampler(sampler);
    scene.Context->Image(texture);
    const Capture img = Render(scene.Target, 3);
    REQUIRE(!img.IsEmpty());
    // the first row of the image is the top of the quad
    CHECK_PIXEL(img, 0.25f, 0.25f, Red, 3);
    CHECK_PIXEL(img, 0.75f, 0.25f, Green, 3);
    CHECK_PIXEL(img, 0.25f, 0.75f, Blue, 3);
    CHECK_PIXEL(img, 0.75f, 0.75f, (u8v4{255, 255, 255, 255}), 3);
}

ONYX_TEST(render, render_textures_feed_other_scenes,
          .Description = "A render texture can be drawn as a texture in another scene")
{
    Scene<D2> source{{32, 32}};
    source.View->ClearColor = Onyx::Color_Red;

    Scene<D2> scene{{64, 64}};
    scene.Camera.OrthoParameters.Size = 1.f;
    scene.Context->Image(*source.Target);
    // the sampled image is the one rendered last frame: give it a couple
    const Capture img = Render(scene.Target, 4);
    REQUIRE(!img.IsEmpty());
    CHECK_PIXEL(img, 0.5f, 0.5f, Red, 3);
}

ONYX_TEST(render, render_textures_resize,
          .Description = "A resized render texture renders and reads back at its new size, its views following it")
{
    Scene<D2> scene{{64, 64}};
    scene.Camera.OrthoParameters.Size = 2.f;
    scene.Context->FillColor(Onyx::Color_Red);
    scene.Context->Quad();
    Render(scene.Target);

    scene.Target->Resize({48, 24});
    const Capture img = Render(scene.Target);
    CHECK(img.Size == u32v2(48, 24));
    CHECK_THAT(scene.View->GetViewport().Size, IsNear(f32v2(48.f, 24.f), 0.5f));
    CHECK_PIXEL(img, 0.5f, 0.5f, Red, 3);
    CHECK_PIXEL(img, 0.05f, 0.5f, Black, 3);
}

ONYX_TEST(render, content_scale_multiplies_pixels,
          .Description = "A render texture with content scale 2 has twice the pixels per axis and the same picture")
{
    Onyx::RenderTexture *rt = TestRenderTexture({32, 32}, {.ContentScale = 2.f});
    Onyx::Camera<D2> cam{};
    cam.OrthoParameters.Size = 2.f;
    Onyx::RenderView<D2> *rv = rt->CreateRenderView<D2>(&cam);
    Onyx::RenderContext<D2> *ctx = TestContext<D2>();
    ctx->Flush();
    ctx->AddTarget(rv);
    ctx->FillColor(Onyx::Color_Red);
    ctx->Quad();

    const Capture img = Render(rt);
    CHECK(rt->GetPixelSize() == u32v2(64, 64));
    CHECK(img.Size == u32v2(64, 64));
    const u32 covered = img.Count(Red, 8);
    CHECK_MSG(covered > 900 && covered < 1150, "{} of 4096 pixels are red, about a quarter expected", covered);
}

ONYX_TEST(render, fxaa_smooths_edges_only,
          .Description = "FXAA blends the edges of a rotated shape and leaves its inside untouched")
{
    Scene<D2> scene{{64, 64}, {.Flags = Onyx::RenderViewFlag_FXAA}};
    scene.Camera.OrthoParameters.Size = 2.f;
    scene.Context->FillColor(Onyx::Color_White);
    scene.Context->Rotate(0.4f);
    scene.Context->Quad();
    const Capture img = Render(scene.Target);
    REQUIRE(!img.IsEmpty());
    CHECK_PIXEL(img, 0.5f, 0.5f, (u8v4{255, 255, 255, 255}), 3);
    CHECK_MSG(countBetween(img, Black, u8v4{255, 255, 255, 255}) > 0, "No blended edge pixel");
}

ONYX_TEST(render, msaa_renders_correctly,
          .Description = "With 4x multisampling the scene renders as without it, with blended edges",
          .Init = Init_Manual)
{
    Onyx::Specs specs{};
    specs.SampleCount = Graph::SampleCount_4;
    Onyx::Initialize(specs);
    Onyx::Resources_CreateDefault();
    {
        Scene<D2> scene{{64, 64}};
        scene.Camera.OrthoParameters.Size = 2.f;
        scene.Context->FillColor(Onyx::Color_White);
        scene.Context->Rotate(0.4f);
        scene.Context->Quad();
        const Capture img = Render(scene.Target);
        CHECK(!img.IsEmpty());
        if (!img.IsEmpty())
        {
            CHECK_PIXEL(img, 0.5f, 0.5f, (u8v4{255, 255, 255, 255}), 3);
            CHECK_PIXEL(img, 0.05f, 0.05f, Black, 3);
            CHECK_MSG(countBetween(img, Black, u8v4{255, 255, 255, 255}) > 0, "No multisampled edge pixel");
        }
    }
    // a failed REQUIRE ends the process before this. failed CHECKs leave Onyx in one piece
    Onyx::Terminate();
}

ONYX_TEST(render, dynamic_meshes_draw_their_vertices,
          .Description = "A dynamic mesh built from vertices on the fly is drawn where its vertices are")
{
    Scene<D2> scene{};
    scene.Camera.OrthoParameters.Size = 2.f;
    Onyx::RenderContext<D2> *ctx = TestContext<D2>({.ImmediateDynamicMeshCapacity = 1});
    ctx->Flush();
    ctx->AddTarget(scene.View);
    // a triangle covering most of the right half
    const f32v2 vertices[3] = {{0.1f, -0.9f}, {0.9f, -0.9f}, {0.9f, 0.9f}};
    ctx->FillColor(Onyx::Color_Green);
    // no sync here: syncing dynamic meshes reflushes every context. the context synced its slot when it was created
    ctx->DynamicMesh(TKit::Span<const f32v2>{vertices});
    const Capture img = Render(scene.Target, 3);
    REQUIRE(!img.IsEmpty());
    CHECK_PIXEL(img, 0.85f, 0.6f, Green, 3);
    CHECK_PIXEL(img, 0.25f, 0.5f, Black, 3);
}
