// Resources: registration, read back and lifetime of materials, meshes, images, textures, samplers and fonts, and the
// loaders (OBJ, images, glTF), fed with small files the tests write themselves so no assets are needed.

#include "harness.hpp"
#include "onyx/font.hpp"
#include "onyx/gltf.hpp"
#include "onyx/image.hpp"
#include "onyx/material.hpp"
#include "onyx/mesh.hpp"
#include <cstring>
#include <vector>

using namespace OnyxTest;

ONYX_TEST(resources, materials_read_back,
          .Description = "A registered material reads back unchanged, and updates replace it")
{
    const Onyx::Resource m2 = Onyx::Material_Register<D2>({.ColorFactor = Onyx::Color_Cyan, .Occluder = true});
    REQUIRE(Onyx::IsResourceValid<D2>(m2, Onyx::Resource_Material));
    CHECK(Onyx::Material_GetData<D2>(m2).Occluder);
    CHECK_THAT(Onyx::Material_GetData<D2>(m2).ColorFactor.rgba, IsNear(Onyx::Color_Cyan.rgba, 1e-6f));

    Onyx::Material_Update<D2>(m2, {.ColorFactor = Onyx::Color_Red});
    CHECK(!Onyx::Material_GetData<D2>(m2).Occluder);
    CHECK_THAT(Onyx::Material_GetData<D2>(m2).ColorFactor.rgba, IsNear(Onyx::Color_Red.rgba, 1e-6f));

    const Onyx::Resource m3 = Onyx::Material_Register<D3>();
    REQUIRE(Onyx::IsResourceValid<D3>(m3, Onyx::Resource_Material));
    Onyx::Resources_Sync(Onyx::SyncFlag_Materials);
    Onyx::Material_Destroy<D3>(m3);
    CHECK(!Onyx::IsResourceValid<D3>(m3, Onyx::Resource_Material));
}

ONYX_TEST(resources, static_meshes_read_back,
          .Description = "A mesh registered in a pool reads back with the same vertices and indices")
{
    const Onyx::ResourcePool pool = Onyx::ResourcePool_Create<D2>(Onyx::Resource_StaticMesh);
    const Onyx::StaticMeshData<D2> data = Onyx::Mesh_CreateRegularPolygonData<D2>(6);
    const Onyx::Resource mesh = Onyx::ResourcePool_RegisterMesh<D2>(pool, data);
    Onyx::Resources_Sync(Onyx::SyncFlag_StaticMeshes);
    REQUIRE(Onyx::IsResourceValid<D2>(mesh, Onyx::Resource_StaticMesh));
    CHECK((Onyx::ResourcePool_GetResourceCount<D2>(pool)) == 1u);

    const Onyx::StaticMeshData<D2> back = Onyx::Mesh_GetDataStatic<D2>(mesh);
    CHECK(back.Vertices.GetSize() == data.Vertices.GetSize());
    CHECK(back.Indices.GetSize() == data.Indices.GetSize());
    const Onyx::MeshRange range = Onyx::Mesh_GetRange<D2>(mesh);
    CHECK(range.VertexCount == data.Vertices.GetSize());
    CHECK(range.IndexCount == data.Indices.GetSize());
}

ONYX_TEST(resources, dynamic_meshes_register_and_destroy,
          .Description = "Dynamic meshes are counted while they live and freed on destroy")
{
    const u32 before = Onyx::DynamicMesh_GetCount<D2>();
    const Onyx::Resource mesh = Onyx::DynamicMesh_Register<D2>();
    CHECK(Onyx::IsResourceValid<D2>(mesh, Onyx::Resource_DynamicMesh));
    CHECK((Onyx::DynamicMesh_GetCount<D2>()) == (before + 1));
    Onyx::DynamicMesh_Destroy<D2>(mesh);
    CHECK((Onyx::DynamicMesh_GetCount<D2>()) == before);
}

ONYX_TEST(resources, images_textures_and_samplers_live_and_die,
          .Description = "Images, textures and samplers are valid once created and invalid once destroyed")
{
    u8 pixels[4 * 4] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    Onyx::ImageData data{};
    data.Data = reinterpret_cast<std::byte *>(pixels);
    data.Width = 2;
    data.Height = 2;
    data.Components = 4;
    data.Format = Graph::Format_R8G8B8A8_SRGB;
    CHECK(data.ComputeSize() == usz(16));

    const Onyx::Resource image = Onyx::Image_Create(data);
    REQUIRE(Onyx::IsResourceValid(image, Onyx::Resource_Image));
    const Onyx::Resource texture = Onyx::Texture_Create(image);
    REQUIRE(Onyx::IsResourceValid(texture, Onyx::Resource_Texture));
    const Onyx::Resource sampler = Onyx::Sampler_Create({.MagFilter = Graph::Filter_Nearest});
    REQUIRE(Onyx::IsResourceValid(sampler, Onyx::Resource_Sampler));
    Onyx::Resources_Sync(Onyx::SyncFlag_Images | Onyx::SyncFlag_Textures | Onyx::SyncFlag_Samplers);
    Frame(1);

    Onyx::Texture_Destroy(texture);
    Onyx::Image_Destroy(image);
    Onyx::Sampler_Destroy(sampler);
    Onyx::Resources_Sync(Onyx::SyncFlag_All);
    CHECK(!Onyx::IsResourceValid(texture, Onyx::Resource_Texture));
    CHECK(!Onyx::IsResourceValid(image, Onyx::Resource_Image));
    CHECK(!Onyx::IsResourceValid(sampler, Onyx::Resource_Sampler));
}

#ifdef ONYX_ENABLE_FONT_LOAD
ONYX_TEST(resources, default_font_covers_printable_ascii,
          .Description = "The default font has a glyph for every printable ASCII character, each pointing back to it")
{
    const Onyx::Resource font = Onyx::Resources_GetDefault().Font;
    REQUIRE(Onyx::IsResourceValid(font, Onyx::Resource_Font));
    u32 missing = 0;
    for (Onyx::CodePoint c = 32; c < 127; ++c)
    {
        if (!Onyx::Font_HasGlyph(font, c))
        {
            ++missing;
            continue;
        }
        const Onyx::Resource glyph = Onyx::Font_GetGlyph(font, c);
        CHECK_MSG(Onyx::Glyph_GetFont(glyph) == font, "Glyph {} belongs to another font", c);
    }
    CHECK_MSG(missing == 0, "{} printable ASCII characters have no glyph", missing);
}

ONYX_TEST(resources, default_font_metrics_are_sane,
          .Description = "Ascender above the baseline, descender below, and a line height that holds both")
{
    const Onyx::FontInfo &info = Onyx::Font_GetInfo(Onyx::Resources_GetDefault().Font);
    CHECK(info.Ascender > 0.f);
    CHECK(info.Descender < 0.f);
    CHECK(info.LineHeight >= info.Ascender - info.Descender - 1e-3f);
    CHECK(info.UnitRange > 0.f);
    CHECK(!info.Glyphs.IsEmpty());
}
#endif

#ifdef ONYX_ENABLE_OBJ_LOAD
ONYX_TEST(resources, obj_files_load,
          .Description = "An OBJ file loads into a static mesh with its vertices and triangles")
{
    const char *obj = "# a unit square in the xy plane\n"
                      "v -0.5 -0.5 0\nv 0.5 -0.5 0\nv 0.5 0.5 0\nv -0.5 0.5 0\n"
                      "vn 0 0 1\n"
                      "f 1//1 2//1 3//1\nf 1//1 3//1 4//1\n";
    const Path path = TempPath("square.obj");
    REQUIRE(WriteFile(path, obj, std::strlen(obj)));
    const auto result = Onyx::Mesh_LoadDataFromObjFile<D3>(path);
    REQUIRE_MSG(result, "{}", Describe(result));

    const Onyx::StaticMeshData<D3> &data = *result;
    CHECK(data.Indices.GetSize() == 6u);
    CHECK((data.Vertices.GetSize() >= 4 && data.Vertices.GetSize() <= 6));

    const Onyx::ResourcePool pool = Onyx::ResourcePool_Create<D3>(Onyx::Resource_StaticMesh);
    const Onyx::Resource mesh = Onyx::ResourcePool_RegisterMesh<D3>(pool, data);
    Onyx::Resources_Sync(Onyx::SyncFlag_StaticMeshes);
    CHECK(Onyx::IsResourceValid<D3>(mesh, Onyx::Resource_StaticMesh));
}
#endif

#ifdef ONYX_ENABLE_IMAGE_LOAD
// a 2x2 24 bit BMP: red, green on top, blue, white at the bottom. BMP rows are stored bottom up and padded to 4 bytes
static Array<u8> makeBmp()
{
    Array<u8> bmp{};
    bmp.Resize(54 + 16, u8(0));
    const auto put32 = [&](const usize at, const u32 v) {
        for (u32 i = 0; i < 4; ++i)
            bmp[at + i] = u8(v >> (8 * i));
    };
    bmp[0] = 'B';
    bmp[1] = 'M';
    put32(2, u32(bmp.GetSize()));
    put32(10, 54);
    put32(14, 40);
    put32(18, 2);
    put32(22, 2);
    bmp[26] = 1;
    bmp[28] = 24;
    put32(34, 16);
    const u8 rows[16] = {255, 0, 0,   255, 255, 255, 0, 0,  // bottom: blue, white (BGR), padding
                         0,   0, 255, 0,   255, 0,   0, 0}; // top: red, green (BGR), padding
    for (u32 i = 0; i < 16; ++i)
        bmp[54 + i] = rows[i];
    return bmp;
}

ONYX_TEST(resources, images_load_from_memory,
          .Description = "An encoded image decodes to the right size and pixels, top row first")
{
    const Array<u8> bmp = makeBmp();
    const auto result = Onyx::Image_LoadDataFromMemory(reinterpret_cast<const std::byte *>(bmp.GetData()),
                                                       u32(bmp.GetSize()), Onyx::ImageComponent_RGBA);
    REQUIRE_MSG(result, "{}", Describe(result));
    const Onyx::ImageData &data = *result;
    CHECK(data.Width == 2u);
    CHECK(data.Height == 2u);
    CHECK(data.Components == 4u);
    const u8 *px = reinterpret_cast<const u8 *>(data.Data);
    const u8v4 expected[4] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}, {255, 255, 255, 255}};
    for (u32 i = 0; i < 4; ++i)
    {
        const u8v4 got{px[4 * i], px[4 * i + 1], px[4 * i + 2], px[4 * i + 3]};
        CHECK_MSG(Near(got, expected[i], 0), "Pixel {} is {}, expected {}", i, got, expected[i]);
    }
    Onyx::Image_UnloadData(data);
}

ONYX_TEST(resources, corrupt_images_report_an_error,
          .Description = "Decoding garbage returns an error instead of crashing")
{
    const u8 garbage[32] = {1, 2, 3, 4, 5, 6, 7, 8};
    const auto result =
        Onyx::Image_LoadDataFromMemory(reinterpret_cast<const std::byte *>(garbage), u32(sizeof(garbage)));
    CHECK(!result);
}
#endif

#ifdef ONYX_ENABLE_GLTF_LOAD
static String base64(const Array<u8> &bytes)
{
    constexpr const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String out{};
    for (usize i = 0; i < bytes.GetSize(); i += 3)
    {
        const u32 b0 = bytes[i];
        const u32 b1 = i + 1 < bytes.GetSize() ? bytes[i + 1] : 0;
        const u32 b2 = i + 2 < bytes.GetSize() ? bytes[i + 2] : 0;
        const u32 triple = (b0 << 16) | (b1 << 8) | b2;
        out += alphabet[(triple >> 18) & 63];
        out += alphabet[(triple >> 12) & 63];
        out += i + 1 < bytes.GetSize() ? alphabet[(triple >> 6) & 63] : '=';
        out += i + 2 < bytes.GetSize() ? alphabet[triple & 63] : '=';
    }
    return out;
}

// Fails until fixed: gltf.cpp builds primToMeshIndex with braces, StackHashMap<i32, u32> m{2048}: that picks the
// initializer list constructor (one element, no buckets) and the first insert asserts. Parentheses, m(2048), give the
// intended capacity
ONYX_TEST(resources, gltf_files_load,
          .Description = "A glTF file with one triangle loads into one static mesh with three vertices")
{
    // indices (3 x u16, padded to 8 bytes) followed by three vec3 positions
    Array<u8> buffer{};
    buffer.Resize(8 + 36, u8(0));
    const u16 indices[3] = {0, 1, 2};
    std::memcpy(buffer.GetData(), indices, sizeof(indices));
    const f32 positions[9] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    std::memcpy(buffer.GetData() + 8, positions, sizeof(positions));

    const String gltf = String::Format(
        R"({{"asset":{{"version":"2.0"}},"scene":0,"scenes":[{{"nodes":[0]}}],"nodes":[{{"mesh":0}}],)"
        R"("meshes":[{{"primitives":[{{"attributes":{{"POSITION":1}},"indices":0}}]}}],)"
        R"("buffers":[{{"uri":"data:application/octet-stream;base64,{}","byteLength":{}}}],)"
        R"("bufferViews":[{{"buffer":0,"byteOffset":0,"byteLength":6,"target":34963}},)"
        R"({{"buffer":0,"byteOffset":8,"byteLength":36,"target":34962}}],)"
        R"("accessors":[{{"bufferView":0,"componentType":5123,"count":3,"type":"SCALAR","max":[2],"min":[0]}},)"
        R"({{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3","max":[1,1,0],"min":[0,0,0]}}]}})",
        base64(buffer), buffer.GetSize());

    const Path path = TempPath("triangle.gltf");
    REQUIRE(WriteFile(path, gltf.GetData(), gltf.GetSize()));
    const auto result = Onyx::Gltf_LoadDataFromFile<D3>(path);
    REQUIRE_MSG(result, "{}", Describe(result));
    REQUIRE(result->StaticMeshes.GetSize() == 1u);
    CHECK(result->StaticMeshes[0].Vertices.GetSize() == 3u);
    CHECK(result->StaticMeshes[0].Indices.GetSize() == 3u);
}
#endif

// ---------------------------------------------------------------------------------------------------------------------
//  Loader errors. Every loader checks its path before reading, so a missing file and a directory are told apart from a
//  file it could not read, and the message names the path
// ---------------------------------------------------------------------------------------------------------------------

struct LoaderPaths
{
    Path Missing;
    Path Directory;
    Path Junk;
};
// a path that does not exist, a directory, and a file no loader can read
static bool makeLoaderPaths(LoaderPaths &paths)
{
    paths.Missing = TempPath("missing-file");
    paths.Directory = TempPath("a-directory");
    paths.Junk = TempPath("junk-file");
    return MakeDirectory(paths.Directory) && WriteFile(paths.Junk, "this is not a file any loader can read\n");
}
// empty when the load failed with the expected error, otherwise what happened instead
template <typename R> static String unexpected(const R &result, const Onyx::ErrorCode expected)
{
    if (result)
        return "it loaded";
    if (result.GetError().GetCode() != expected)
        return result.GetError().ToString();
    return String{};
}
template <typename R> static bool namesPath(const R &result, const Path &path)
{
    return !result && result.GetError().GetMessage().Contains(path.CString());
}
#define EXPECT_LOAD_ERROR(expression, code, what)                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        const String unexpectedOutcome = unexpected((expression), code);                                               \
        CHECK_MSG(unexpectedOutcome.IsEmpty(), "{}: expected {}, got: {}", what, Onyx::ToString(code),                 \
                  unexpectedOutcome);                                                                                  \
    } while (false)

ONYX_TEST(resources, loaders_tell_missing_files_from_directories,
          .Description = "The image, OBJ and font loaders report a missing file and a directory with errors of their "
                         "own, and name the path in the message")
{
    LoaderPaths p{};
    REQUIRE(makeLoaderPaths(p));
#ifdef ONYX_ENABLE_IMAGE_LOAD
    EXPECT_LOAD_ERROR(Onyx::Image_LoadDataFromFile(p.Missing), Onyx::Error_FileNotFound, "image, missing");
    EXPECT_LOAD_ERROR(Onyx::Image_LoadDataFromFile(p.Directory), Onyx::Error_IsNotFile, "image, directory");
    CHECK_MSG(namesPath(Onyx::Image_LoadDataFromFile(p.Missing), p.Missing), "The image error does not name the path");
#endif
#ifdef ONYX_ENABLE_OBJ_LOAD
    EXPECT_LOAD_ERROR(Onyx::Mesh_LoadDataFromObjFile<D3>(p.Missing), Onyx::Error_FileNotFound, "OBJ, missing");
    EXPECT_LOAD_ERROR(Onyx::Mesh_LoadDataFromObjFile<D3>(p.Directory), Onyx::Error_IsNotFile, "OBJ, directory");
    CHECK_MSG(namesPath(Onyx::Mesh_LoadDataFromObjFile<D3>(p.Missing), p.Missing),
              "The OBJ error does not name the path");
#endif
#ifdef ONYX_ENABLE_FONT_LOAD
    EXPECT_LOAD_ERROR(Onyx::Font_LoadDataFromFile(p.Missing), Onyx::Error_FileNotFound, "font, missing");
    EXPECT_LOAD_ERROR(Onyx::Font_LoadDataFromFile(p.Directory), Onyx::Error_IsNotFile, "font, directory");
    CHECK_MSG(namesPath(Onyx::Font_LoadDataFromFile(p.Missing), p.Missing), "The font error does not name the path");
#endif
}

ONYX_TEST(resources, loaders_reject_files_they_cannot_read,
          .Description = "A file a loader cannot parse fails with Error_LoadFailed instead of loading something")
{
    LoaderPaths p{};
    REQUIRE(makeLoaderPaths(p));
#ifdef ONYX_ENABLE_IMAGE_LOAD
    EXPECT_LOAD_ERROR(Onyx::Image_LoadDataFromFile(p.Junk), Onyx::Error_LoadFailed, "image");
#endif
#ifdef ONYX_ENABLE_OBJ_LOAD
    EXPECT_LOAD_ERROR(Onyx::Mesh_LoadDataFromObjFile<D3>(p.Junk), Onyx::Error_LoadFailed, "OBJ");
#endif
#ifdef ONYX_ENABLE_GLTF_LOAD
    EXPECT_LOAD_ERROR(Onyx::Gltf_LoadDataFromFile<D3>(p.Junk), Onyx::Error_LoadFailed, "glTF");
#endif
#ifdef ONYX_ENABLE_FONT_LOAD
    // FreeType gives no reason, and the font loader reports Error_Unknown for it: only the failure is checked
    CHECK_MSG(!Onyx::Font_LoadDataFromFile(p.Junk), "Font: an unreadable file loaded");
#endif
}
