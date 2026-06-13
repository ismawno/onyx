#pragma once

#include "onyx/handle.hpp"
#include "onyx/image.hpp"
#include "onyx/sampler.hpp"
#include "onyx/mesh.hpp"
#include "onyx/gltf.hpp"
#include "onyx/font.hpp"
#include "onyx/material.hpp"

namespace Onyx
{
using SyncFlags = u8;
enum SyncFlagBit : SyncFlags
{
    SyncFlag_StaticMeshes = 1U << 0,
    SyncFlag_ParametricMeshes = 1U << 1,
    // doesnt actually upload anything to the gpu, but it is needed so that sync triggers a reflush of all contexts and
    // instance data arrays have enough space to index into the new dynamic mesh slot
    SyncFlag_DynamicMeshes = 1U << 2,
    //
    SyncFlag_Materials = 1U << 3,
    SyncFlag_Fonts = 1U << 4,
    SyncFlag_Samplers = 1U << 5,
    SyncFlag_Images = 1U << 6,
    SyncFlag_Textures = 1U << 7,
    SyncFlag_All = TKIT_U8_MAX,
};

struct DefaultResources
{
    ResourcePool StaticPool2 = NullHandle;
    ResourcePool ParametricPool2 = NullHandle;

    ResourcePool StaticPool3 = NullHandle;
    ResourcePool ParametricPool3 = NullHandle;

    ResourcePool FontPool = NullHandle;

    Resource Font = NullHandle;
    Resource Sampler = NullHandle;

    Resource Triangle2 = NullHandle;
    Resource Triangle3 = NullHandle;

    Resource Quad2 = NullHandle;
    Resource Quad3 = NullHandle;

    Resource Box = NullHandle;
    Resource Sphere = NullHandle;
    Resource Cylinder = NullHandle;

    Resource Stadium2 = NullHandle;
    Resource Stadium3 = NullHandle;

    Resource RoundedRect2 = NullHandle;
    Resource RoundedRect3 = NullHandle;

    Resource Capsule = NullHandle;
    Resource RoundedBox = NullHandle;
    Resource Torus = NullHandle;

    template <Dimension D> Resource GetTriangle() const
    {
        if constexpr (D == D2)
            return Triangle2;
        else
            return Triangle3;
    }
    template <Dimension D> Resource GetQuad() const
    {
        if constexpr (D == D2)
            return Quad2;
        else
            return Quad3;
    }
    template <Dimension D> Resource GetStadium() const
    {
        if constexpr (D == D2)
            return Stadium2;
        else
            return Stadium3;
    }
    template <Dimension D> Resource GetRoundedRect() const
    {
        if constexpr (D == D2)
            return RoundedRect2;
        else
            return RoundedRect3;
    }
    template <Dimension D> ResourcePool GetStaticPool() const
    {
        if constexpr (D == D2)
            return StaticPool2;
        else
            return StaticPool3;
    }
    template <Dimension D> ResourcePool GetParametricPool() const
    {
        if constexpr (D == D2)
            return ParametricPool2;
        else
            return ParametricPool3;
    }
};

struct DefaultResourcesOptions
{
    ResourcePool StaticPool2 = NullHandle;
    ResourcePool ParametricPool2 = NullHandle;

    ResourcePool StaticPool3 = NullHandle;
    ResourcePool ParametricPool3 = NullHandle;

    ResourcePool FontPool = NullHandle;
    Resource DefaultFont = NullHandle; // If null, will load default font if enabled

    SamplerData SamplerData{};
#ifdef ONYX_INCLUDE_DEFAULT_FONT
    FontLoadOptions FontOpts{.CharSet = {CharSets[CharSet_ASCII], CharSets[CharSet_GeometricShapes]}};
#endif

    StaticMeshData<D2> TriangleData2 = CreateTriangleMeshData<D2>();
    StaticMeshData<D3> TriangleData3 = CreateTriangleMeshData<D3>();

    StaticMeshData<D2> QuadData2 = CreateQuadMeshData<D2>();
    StaticMeshData<D3> QuadData3 = CreateQuadMeshData<D3>();

    StaticMeshData<D3> BoxData = CreateBoxMeshData();
    StaticMeshData<D3> SphereData = CreateSphereMeshData();
    StaticMeshData<D3> CylinderData = CreateCylinderMeshData();

    ParametricMeshData<D2> StadiumData2 = CreateStadiumMeshData<D2>();
    ParametricMeshData<D3> StadiumData3 = CreateStadiumMeshData<D3>();

    ParametricMeshData<D2> RoundedRectData2 = CreateRoundedRectMeshData<D2>();
    ParametricMeshData<D3> RoundedRectData3 = CreateRoundedRectMeshData<D3>();

    ParametricMeshData<D3> CapsuleData = CreateCapsuleMeshData();
    ParametricMeshData<D3> RoundedBoxData = CreateRoundedBoxMeshData();
    ParametricMeshData<D3> TorusData = CreateTorusMeshData();
};
} // namespace Onyx

namespace Onyx::Resources
{
// NOTE(Isma): No way to create/use buffers in onyx yet through this api
void DestroyBuffer(Resource buffer);
void ReleaseBuffer(Resource buffer);

Resource CreateSampler(const SamplerData &data = {});

// requires a call to Sync() for materials in case any of them referenced the sampler
void DestroySampler(Resource sampler);
//
void UpdateSampler(Resource sampler, const SamplerData &data);
void ReleaseSampler(Resource sampler);

Resource CreateImage(const ImageData &data);
void DestroyImage(Resource image);
void UpdateImage(Resource image, const ImageData &data);
void ReleaseImage(Resource image);

// TODO(Isma): Add options for the texture: mips, array layers, etc
// if view is u32 max, a new view is created from the image
Resource CreateTexture(Resource image, u32 viewIndex = TKIT_U32_MAX);

// requires a call to Sync() for materials in case any of them referenced the texture
void DestroyTexture(Resource texture);
void UpdateTexture(Resource texture, Resource image, u32 viewIndex = TKIT_U32_MAX);
void ReleaseTexture(Resource texture);

const DefaultResources &GetDefaultResources();
const DefaultResources &CreateDefaultResources(const DefaultResourcesOptions &opts = {});

template <Dimension D> DynamicMeshInfo<D> RegisterDynamicMesh();
template <Dimension D> DynamicMeshData<D> *GetDynamicMeshData(Resource mesh);
template <Dimension D> void DestroyDynamicMesh(Resource mesh);

template <Dimension D> Resource RegisterMesh(ResourcePool pool, const StaticMeshData<D> &data);
template <Dimension D> Resource RegisterMesh(ResourcePool pool, const ParametricMeshData<D> &data);
template <Dimension D> Resource RegisterMaterial(const MaterialData<D> &data = {});

// no way to update fonts bc they are amalgamated withing their pool. would require the new font to be the exact same
// size as the old one, which is not pragmatic at all
Resource RegisterFont(ResourcePool pool, const FontData &data);
template <Dimension D> GltfHandles RegisterGltfResources(ResourcePool meshPool, GltfData<D> &data);

template <Dimension D> void UpdateMesh(Resource mesh, const StaticMeshData<D> &data);
template <Dimension D> void UpdateMesh(Resource mesh, const ParametricMeshData<D> &data);
template <Dimension D> void UpdateMaterial(Resource material, const MaterialData<D> &data);

template <Dimension D> ResourcePool CreateResourcePool(ResourceType rtype);
ResourcePool CreateFontPool();

template <Dimension D> void DestroyResourcePool(ResourcePool pool);
template <Dimension D> void ReleaseResourcePool(ResourcePool pool);

void DestroyFontPool(ResourcePool pool);
void ReleaseFontPool(ResourcePool pool);

template <Dimension D> void DestroyMaterial(Resource material);

template <Dimension D> StaticMeshData<D> GetStaticMeshData(Resource mesh);
template <Dimension D> ParametricMeshData<D> GetParametricMeshData(Resource mesh);
template <Dimension D> ParametricShape GetParametricShape(Resource mesh);

template <Dimension D> const MaterialData<D> &GetMaterialData(Resource material);
template <Dimension D> TKit::Span<const u32> GetResourcePoolIds(ResourceType rtype);

TKit::Span<const u32> GetFontPoolIds();
const FontData &GetFontData(Resource font);

Resource GetFontAtlas(Resource font);
Resource GetFont(Resource glyph);
Resource GetGlyph(Resource font, CodePoint codePoint);
const GlyphData &GetGlyphData(Resource glyph);

template <Dimension D> u32 GetDistinctBatchDrawCount();

template <Dimension D> u32 GetResourceCount(ResourcePool pool);
template <Dimension D> u32 GetDynamicMeshCount();

template <Dimension D> MeshDataLayout GetMeshLayout(Resource mesh);
template <Dimension D> Resource GetMeshBounds(Resource mesh);
template <Dimension D> const BoundsData<D> &GetBoundsData(Resource bounds);

u32 GetFontCount(ResourcePool pool);
u32 GetGlyphCount(ResourcePool pool);

MeshDataLayout GetFontLayout(Resource font);
MeshDataLayout GetGlyphLayout(Resource glyph);

template <Dimension D> bool IsResourceValid(Resource handle, ResourceType rtype = Resource_None);
template <Dimension D> bool IsResourcePoolValid(Handle handle, ResourceType rtype = Resource_None);

bool IsResourceValid(Resource handle, ResourceType rtype = Resource_None);
bool IsResourcePoolValid(ResourcePool handle, ResourceType rtype = Resource_None);

void Sync(SyncFlags flags);

} // namespace Onyx::Resources
