#pragma once

#include "onyx/dimension.hpp"
#include "onyx/handle.hpp"
#include "onyx/pass.hpp"
#include "graph/resources.hpp"
#include "tkit/container/bitset.hpp"

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
    Geometry_Glyph,
    Geometry_Dynamic,
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
    u32 Instances = 0;
    u32 Capacity = 0;
};

struct LocalResourceRegistry
{
    // NOTE(Isma): Is this enough?
    TKit::TierArray<u32> ResourceIds{};
    TKit::StaticBitSet2048 UsedResourceIds{2048};

    void Clear()
    {
        ResourceIds.Clear();
        UsedResourceIds.ClearAll();
    }

    void RegisterResourceId(const u32 rid)
    {
        if (!UsedResourceIds[rid])
        {
            UsedResourceIds.Set(rid);
            ResourceIds.Append(rid);
        }
    }
};

struct InstanceResourceGroup
{
    TKit::TierArray<InstanceHostBuffer> Buffers{};
    LocalResourceRegistry Registry{};
};

struct ContextInstanceData
{
    ten<InstanceHostBuffer, BlendPass_Count, RenderMode_Count> Circles{};
    ten<InstanceResourceGroup, BlendPass_Count, RenderMode_Count> DynamicMeshes{};
    ten<InstanceResourceGroup, BlendPass_Count, RenderMode_Count, Resource_MeshPoolCount, ONYX_MAX_RESOURCE_POOLS>
        Meshes{};
};

} // namespace Onyx
