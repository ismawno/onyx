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
    SyncFlag_Materials = 1U << 2,
    SyncFlag_Fonts = 1U << 3,
    SyncFlag_Samplers = 1U << 4,
    SyncFlag_Images = 1U << 5,
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
    Resource FontSampler = NullHandle;

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

    SamplerData FontSamplerData{};
#ifdef ONYX_INCLUDE_DEFAULT_FONT
    FontLoadOptions FontOpts{};
#endif

    StatMeshData<D2> TriangleData2 = CreateTriangleMeshData<D2>();
    StatMeshData<D3> TriangleData3 = CreateTriangleMeshData<D3>();

    StatMeshData<D2> QuadData2 = CreateQuadMeshData<D2>();
    StatMeshData<D3> QuadData3 = CreateQuadMeshData<D3>();

    StatMeshData<D3> BoxData = CreateBoxMeshData();
    StatMeshData<D3> SphereData = CreateSphereMeshData();
    StatMeshData<D3> CylinderData = CreateCylinderMeshData();

    ParaMeshData<D2> StadiumData2 = CreateStadiumMeshData<D2>();
    ParaMeshData<D3> StadiumData3 = CreateStadiumMeshData<D3>();

    ParaMeshData<D2> RoundedRectData2 = CreateRoundedRectMeshData<D2>();
    ParaMeshData<D3> RoundedRectData3 = CreateRoundedRectMeshData<D3>();

    ParaMeshData<D3> CapsuleData = CreateCapsuleMeshData();
    ParaMeshData<D3> RoundedBoxData = CreateRoundedBoxMeshData();
    ParaMeshData<D3> TorusData = CreateTorusMeshData();
};
} // namespace Onyx

namespace Onyx::Resources
{
// NOTE(Isma): No way to create/use buffers in onyx yet

void DestroyBuffer(Resource buffer);
void ReleaseBuffer(Resource buffer);

Resource CreateSampler(const SamplerData &data = {});
void DestroySampler(Resource sampler);
void ReleaseSampler(Resource sampler);

Resource CreateImage(const ImageData &data);
void DestroyImage(Resource image);
void ReleaseImage(Resource image);

// TODO(Isma): Add options for the texture: mips, array layers, etc
Resource CreateTexture(Resource image, u32 viewIndex = TKIT_U32_MAX);
void DestroyTexture(Resource texture);

// TODO(Isma): Create a utility function that registers and loads all default resources. Create internally a struct with
// default resources to be used by contexts. Contexts refresh default resources every flush, so the sync flush will
// naturally refresh all defaults

const DefaultResources &GetDefaultResources();
const DefaultResources &CreateDefaultResources(const DefaultResourcesOptions &opts = {});

template <Dimension D> Resource RegisterMesh(ResourcePool pool, const StatMeshData<D> &data);
template <Dimension D> Resource RegisterMesh(ResourcePool pool, const ParaMeshData<D> &data);
template <Dimension D> Resource RegisterMaterial(const MaterialData<D> &data = {});

Resource RegisterFont(ResourcePool pool, const FontData &data);
template <Dimension D> GltfHandles RegisterGltfResources(ResourcePool meshPool, GltfData<D> &data);

template <Dimension D> void UpdateMesh(Resource mesh, const StatMeshData<D> &data);
template <Dimension D> void UpdateMesh(Resource mesh, const ParaMeshData<D> &data);
template <Dimension D> void UpdateMaterial(Resource material, const MaterialData<D> &data);

template <Dimension D> ResourcePool CreateResourcePool(ResourceType rtype);
ResourcePool CreateFontPool();

template <Dimension D> void DestroyResourcePool(ResourcePool pool);
template <Dimension D> void ReleaseResourcePool(ResourcePool pool);

void DestroyFontPool(ResourcePool pool);
void ReleaseFontPool(ResourcePool pool);

template <Dimension D> void DestroyMaterial(Resource material);

template <Dimension D> StatMeshData<D> GetStaticMeshData(Resource mesh);
template <Dimension D> ParaMeshData<D> GetParametricMeshData(Resource mesh);
template <Dimension D> ParametricShape GetParametricShape(Resource mesh);

template <Dimension D> const MaterialData<D> &GetMaterialData(Resource material);
template <Dimension D> TKit::Span<const u32> GetResourcePoolIds(ResourceType rtype);

TKit::Span<const u32> GetFontPoolIds();
const FontData &GetFontData(Resource font);

Resource GetFontAtlas(Resource font);
const Glyph *GetGlyph(Resource font, u32 codePoint);

template <Dimension D> u32 GetDistinctBatchDrawCount();

template <Dimension D> u32 GetResourceCount(ResourcePool pool);

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
