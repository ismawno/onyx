#pragma once

#include "onyx/dimension.hpp"
#include "onyx/handle.hpp"
#include "onyx/state.hpp"
#include "graph/resources.hpp"
#include "tkit/container/bit_set.hpp"
#include "tkit/container/bit_field_map.hpp"

// NOTE(Isma): At some point ill have to handle user wanting to explicitly submit instance data buffers from gpu
// (without any cpu detours)
namespace Onyx
{
struct ArcData
{
    f32 LowerCos;
    f32 LowerSin;
    f32 UpperCos;
    f32 UpperSin;
};
struct FadeData
{
    u32 AngleOverflow;
    f32 Hollowness;
    f32 InnerFade;
    f32 OuterFade;
};

enum ParametricShape : u32
{
    ParametricShape_Stadium,
    ParametricShape_RoundedRect,
    ParametricShape_Capsule,
    ParametricShape_RoundedBox,
    ParametricShape_Torus,
};

struct StadiumParameters
{
    f32 Height;
    f32 Radius;
};

struct RoundedRectParameters
{
    f32 Width;
    f32 Height;
    f32 Radius;
};

struct CapsuleParameters
{
    f32 Height;
    f32 Radius;
};

struct RoundedBoxParameters
{
    f32 Width;
    f32 Height;
    f32 Length;
    f32 Radius;
};

struct TorusParameters
{
    f32 InnerRadius;
    f32 OuterRadius;
};

union InstanceParameters {
    StadiumParameters Stadium;
    RoundedRectParameters RoundedRect;
    CapsuleParameters Capsule;
    RoundedBoxParameters RoundedBox;
    TorusParameters Torus;
};

enum Geometry : u8
{
    Geometry_Circle,
    Geometry_Static,
    Geometry_Parametric,
    Geometry_Dynamic,
    Geometry_Glyph,
    Geometry_Count,
};

enum LightType : u8
{
    Light_Point,
    Light_Directional,
    Light_Spot,
    Light_Count
};

using LightFlags = u32;
enum LightFlagBit : LightFlags
{
    LightFlag_CastShadows = 1U << 0,
    LightFlag_PCF = 1U << 1,
    LightFlag_PCSS = 1U << 2,
};

template <Dimension D> constexpr u32 LightTypeCount = D == D2 ? 2 : 3;

// TODO(Isma): Define alignment name that works for all dimensions
enum Alignment : u8
{
    Alignment_Canonical,
    Alignment_Center,
    Alignment_Mirrored,
    Alignment_Left = Alignment_Canonical,
    Alignment_Right = Alignment_Mirrored,
    Alignment_Bottom = Alignment_Canonical,
    Alignment_Top = Alignment_Mirrored,
    Alignment_Near = Alignment_Canonical,
    Alignment_Far = Alignment_Mirrored,
    Alignment_None = 3,
};

template <Dimension D> struct WorldRect
{
    f32v<D> Min;
    f32v<D> Edge0;
    f32v<D> Edge1;
};
template <> struct WorldRect<D3>
{
    f32v3 Min;
    f32v3 Edge0;
    f32v3 Edge1;
    f32v3 Edge2;
};

const char *ToString(Geometry geo);
const char *ToString(LightType light);

class HostBuffer
{
  public:
    HostBuffer(const usize alignment = alignof(std::max_align_t)) : m_Alignment(alignment)
    {
    }
    HostBuffer(const usz size, const usize alignment = alignof(std::max_align_t)) : m_Size(size), m_Alignment(alignment)
    {
        m_Data = TKit::AllocateAligned(size, alignment);
    }

    template <typename T> static HostBuffer Create(const u32 instanceCount)
    {
        return HostBuffer(instanceCount * sizeof(T), alignof(T));
    }

    void Write(const void *data, const Graph::BufferCopy &copy)
    {
        TKIT_ASSERT(m_Size >= copy.Size + copy.DstOffset,
                    "[ONYX][BUFFER] Copy range with size {:L} bytes and dst offset {:L} bytes exceeds buffer bounds by "
                    "{:L} bytes on this buffer with a size of {:L} bytes",
                    copy.Size, copy.DstOffset, copy.Size + copy.DstOffset - m_Size, m_Size);

        std::byte *dst = scast<std::byte *>(m_Data) + copy.DstOffset;
        const std::byte *src = scast<const std::byte *>(data) + copy.SrcOffset;
        TKit::ForwardCopy(dst, src, copy.Size);
    }
    void Destroy()
    {
        TKit::DeallocateAligned(m_Data);
        m_Data = nullptr;
    }

    const void *GetData() const
    {
        return m_Data;
    }
    void *GetData()
    {
        return m_Data;
    }

    void Resize(const usz size)
    {
        void *data = TKit::AllocateAligned(size, m_Alignment);
        if (m_Data)
        {
            TKit::ForwardCopy(data, m_Data, m_Size);
            TKit::DeallocateAligned(m_Data);
        }
        m_Data = data;
        m_Size = size;
    }
    operator bool() const
    {
        return m_Data != nullptr;
    }

    usz GetSize() const
    {
        return m_Size;
    }

  private:
    void *m_Data = nullptr;
    usz m_Size = 0;
    usize m_Alignment;
};

struct InstanceHostBuffer
{
    HostBuffer Data{};
#ifdef ONYX_ENABLE_HOOKS
    HostBuffer HookData{};
#endif
    u32 Instances = 0;
    u32 Capacity = 0;
    bool IsEmpty() const
    {
        return Instances == 0;
    }
};

// groups purpose is to register what ids (either resource or pool ids) have been issued in between Flush() calls for a
// context. it comes in to levels: Mesh and Pool groups. this only applies to meshes, so instead of resource id, ill
// say mesh id
//
// when user submits a mesh id, we want to retrieve its instance data buffer quickly. thats what Buffers field is for.
// we can index into it with the mesh id to get its buffer. however, walking that buffer array is not fun, taking into
// account in most use cases only a handful of ids will be used. so we save a registry of the used resource ids
//
// for the pooled cases, its pretty much the same thing. now, instead of mesh ids we have pool ids, and instead of
// actual buffers, we have InstanceMeshGroups
//
// there is also another thing to take into account: right now, everything is statically allocated. no resizes or
// acommodations are needed because we already operate at the maximum allowed pool and mesh ids. the problem with this
// is that, in the current state of things, ContextInstanceData can go up to 4mb in size with
// ONYX_POSSIBLE_RESOURCE_POOLS to 15 and ONYX_MAX_MESH_IDS_PER_POOL to 256, which are fairly low
//
// we could switch to tier arrays and, every Flush(), resize based on how many pool ids and mesh ids (within said pools)
// exist, so we allocate on demand. this would be doable, if only because of the ContextFieldMap<..., D> fields for
// meshes. entries on these get created (TryInsert calls) on the spot based on the render state the user wants to
// submit. to declare it upfront is not natural. mesh and pool id counts are declared upfront just because of the fact
// they need to be created. but state does not. so instead, we should acommodate for all combinations, which is a bit
// weird and wasteful as well. not as wasteful as this, because we arent allocating for max pools and meshes, only for
// all render state possible combinations, but it increases complexity a lot
template <u32 MaxIds> struct ActiveHandleIdRegistry
{
    TKit::StaticArray<u32, MaxIds> Ids{};
    TKit::StaticBitSet<MaxIds> RegisteredIds{MaxIds};

    void RegisterId(const u32 id)
    {
        if (!RegisteredIds[id])
        {
            RegisteredIds.Set(id);
            Ids.Append(id);
        }
    }

    void Clear()
    {
        Ids.Clear();
        RegisteredIds.ClearAll();
    }

    // void Acommodate(const u32 count)
    // {
    //     RegisteredIds.Resize(count);
    //     Ids.Reserve(count);
    // }
};

struct InstanceMeshGroup
{
    TKit::FixedArray<InstanceHostBuffer, ONYX_MAX_MESH_IDS_PER_POOL> Buffers{};
    ActiveHandleIdRegistry<ONYX_MAX_MESH_IDS_PER_POOL> Registry{};

    void Clear()
    {
        for (const u32 mid : Registry.Ids)
            Buffers[mid].Instances = 0;
        Registry.Clear();
    }

    bool IsEmpty() const
    {
        return Registry.Ids.IsEmpty();
    }

    // void Acommodate(const u32 count)
    // {
    //     Buffers.Resize(count);
    //     Registry.Acommodate(count);
    // }
};

struct InstancePoolGroup
{
    TKit::FixedArray<InstanceMeshGroup, ONYX_POSSIBLE_RESOURCE_POOLS> ResourceGroups{};
    ActiveHandleIdRegistry<ONYX_POSSIBLE_RESOURCE_POOLS> Registry{};

    void Clear()
    {
        for (const u32 pid : Registry.Ids)
            ResourceGroups[pid].Clear();
        Registry.Clear();
    }

    bool IsEmpty() const
    {
        return Registry.Ids.IsEmpty();
    }

    // void Acommodate(const u32 count)
    // {
    //     ResourceGroups.Resize(count);
    //     Registry.Acommodate(count);
    // }
};

template <Dimension D> struct ContextFieldDesc
{
    static constexpr u32 BitCount = RenderState_GeometryBitCount;

    static constexpr RenderState Canonicalize(const RenderState state)
    {
        if constexpr (D == D2)
            return state & ~CullMode_Mask;
        else
            return state;
    }

    static constexpr bool IsValid(const RenderState state)
    {
        const bool hasFill = (state & FillMode_Mask) != FillMode_None;
        const bool validFill = (state & FillMode_Mask) != FillMode_Mask;
        const bool hasOutline = (state & OutlineMode_Mask) != OutlineMode_Disabled;
        const bool validCull =
            D == D2 ? (state & CullMode_Mask) == CullMode_None : (state & CullMode_Mask) != CullMode_Mask;
        return (hasFill || hasOutline) && validCull && validFill;
    }
};

enum PooledMesh : u8
{
    PooledMesh_Static,
    PooledMesh_Parametric,
    PooledMesh_Glyph,
    PooledMesh_Count,
};

template <typename T, Dimension D> using ContextFieldMap = TKit::StaticBitFieldMap<T, ContextFieldDesc<D>>;
template <Dimension D> struct ContextInstanceData
{
    ContextFieldMap<InstanceHostBuffer, D> Circles{};
    ContextFieldMap<InstanceMeshGroup, D> DynamicMeshes{};
    ten<ContextFieldMap<InstancePoolGroup, D>, PooledMesh_Count> Meshes{};
};
} // namespace Onyx
