#pragma once

#include "onyx/alias.hpp"
#include "onyx/definitions.hpp"
#include "tkit/utils/limits.hpp"
#include "tkit/utils/debug.hpp"

// TODO(Isma): Hide these checks
#ifdef TKIT_ENABLE_ASSERTS
#    define ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_TYPE(hndl)                                                            \
        TKIT_ASSERT(                                                                                                   \
            Onyx::Resources::GetResourceTypeAsInteger(hndl) < Onyx::Resource_Count,                                    \
            "[ONYX][RESOURCES] The handle {:#010x} does not match any known resource types ({}), which likely "        \
            "means it is a "                                                                                           \
            "broken handle",                                                                                           \
            hndl, Onyx::Resources::GetResourceTypeAsInteger(hndl))

#    define ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(hndl)                                                       \
        ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_TYPE(hndl);                                                               \
        TKIT_ASSERT(Onyx::Resources::GetResourceTypeAsInteger(hndl) < Onyx::Resource_PoolCount,                        \
                    "[ONYX][RESOURCES] The handle {:#010x} is a '{}' handle, which does not have any resource pool "   \
                    "associated",                                                                                      \
                    hndl, Onyx::ToString(Onyx::Resources::GetResourceType(hndl)))

#    define __ONYX_CHECK_HANDLE_HAS_RESOURCE_TYPE(hndl, rtype)                                                         \
        TKIT_ASSERT(Onyx::Resources::GetResourceType(hndl) == rtype,                                                   \
                    "[ONYX][RESOURCES] The handle {:#010x} is not a '{}' handle, but rather a '{}' handle", hndl,      \
                    Onyx::ToString(rtype), Onyx::ToString(Onyx::Resources::GetResourceType(hndl)))

#    define ONYX_CHECK_HANDLE_HAS_RESOURCE_TYPE(hndl, rtype)                                                           \
        ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_TYPE(hndl);                                                               \
        __ONYX_CHECK_HANDLE_HAS_RESOURCE_TYPE(hndl, rtype)

#    define ONYX_CHECK_HANDLE_HAS_RESOURCE_POOL_TYPE(hndl, rtype)                                                      \
        ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(hndl);                                                          \
        __ONYX_CHECK_HANDLE_HAS_RESOURCE_TYPE(hndl, rtype)

#    define ONYX_CHECK_RESOURCE_IS_NOT_NULL(hndl)                                                                      \
        TKIT_ASSERT(!Onyx::Resources::IsResourceNull(hndl),                                                            \
                    "[ONYX][RESOURCES] The handle {:#010x} has a null resource id", hndl)

#    define ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(hndl)                                                                 \
        TKIT_ASSERT(!Onyx::Resources::IsResourcePoolNull(hndl),                                                        \
                    "[ONYX][RESOURCES] The handle {:#010x} has a null resource pool id", hndl)

#    define ONYX_CHECK_RESOURCE_POOL_ID_IS_NOT_NULL(hndl)                                                              \
        TKIT_ASSERT(!Onyx::Resources::IsResourcePoolIdNull(hndl),                                                      \
                    "[ONYX][RESOURCES] The handle {:#010x} has a null resource pool id", hndl)

#    define ONYX_CHECK_RESOURCE_IS_VALID(hndl, rtype)                                                                  \
        ONYX_CHECK_HANDLE_HAS_RESOURCE_TYPE(hndl, rtype);                                                              \
        TKIT_ASSERT(Onyx::Resources::IsResourceValid(hndl, rtype),                                                     \
                    "[ONYX][RESOURCES] The handle {:#010x} is not a valid '{}' handle", hndl, ToString(rtype))

#    define ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(hndl, rtype, dim)                                                    \
        ONYX_CHECK_HANDLE_HAS_RESOURCE_TYPE(hndl, rtype);                                                              \
        TKIT_ASSERT(Onyx::Resources::IsResourceValid<dim>(hndl, rtype),                                                \
                    "[ONYX][RESOURCES] The handle {:#010x} is not a valid '{}' handle", hndl, ToString(rtype))

#    define ONYX_CHECK_RESOURCE_POOL_IS_VALID(hndl, ptype)                                                             \
        ONYX_CHECK_HANDLE_HAS_RESOURCE_POOL_TYPE(hndl, ptype);                                                         \
        TKIT_ASSERT(Onyx::Resources::IsResourcePoolValid(hndl, ptype),                                                 \
                    "[ONYX][RESOURCES] The handle {:#010x} does not contain a valid '{}' pool handle", hndl,           \
                    ToString(ptype))

#    define ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(hndl, ptype, dim)                                               \
        ONYX_CHECK_HANDLE_HAS_RESOURCE_POOL_TYPE(hndl, ptype);                                                         \
        TKIT_ASSERT(Onyx::Resources::IsResourcePoolValid<dim>(hndl, ptype),                                            \
                    "[ONYX][RESOURCES] The handle {:#010x} does not contain a valid '{}' pool handle", hndl,           \
                    ToString(ptype))

#else
#    define ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_TYPE(...)
#    define ONYX_CHECK_HANDLE_HAS_VALID_RESOURCE_POOL_TYPE(...)
#    define ONYX_CHECK_HANDLE_HAS_RESOURCE_TYPE(...)
#    define ONYX_CHECK_HANDLE_HAS_RESOURCE_POOL_TYPE(...)
#    define ONYX_CHECK_RESOURCE_IS_NOT_NULL(...)
#    define ONYX_CHECK_RESOURCE_POOL_IS_NOT_NULL(...)
#    define ONYX_CHECK_RESOURCE_POOL_ID_IS_NOT_NULL(...)
#    define ONYX_CHECK_RESOURCE_IS_VALID(...)
#    define ONYX_CHECK_RESOURCE_IS_VALID_WITH_DIM(...)
#    define ONYX_CHECK_RESOURCE_POOL_IS_VALID(...)
#    define ONYX_CHECK_RESOURCE_POOL_IS_VALID_WITH_DIM(...)
#endif

namespace Onyx
{
using Handle = u32;
constexpr Handle NullHandle = TKit::Limits<Handle>::Max();

using Resource = Handle;
constexpr Resource NullResource = ONYX_RESOURCE_ID_MASK;

using ResourcePool = Handle;
constexpr ResourcePool NullResourcePool = ONYX_RESOURCE_POOL_ID_MASK;

enum ResourceType : u8
{
    Resource_StaticMesh,
    Resource_ParametricMesh,
    Resource_GlyphMesh,
    Resource_DynamicMesh,
    Resource_Font,
    Resource_Material,
    Resource_Buffer,
    Resource_Bounds,
    Resource_Sampler,
    Resource_Image,
    Resource_Texture,
    Resource_Count,

    // not ideal to have them here but idc too much
    Resource_MeshPoolCount = Resource_DynamicMesh,
    Resource_PoolCount = Resource_Material,
    Resource_None = Resource_Count
};

static_assert(Resource_Count <= ONYX_MAX_RESOURCE_TYPES,
              "[ONYX][RESOURCES] The resource type count exceeds maximum resource types");

const char *ToString(ResourceType rtype);

} // namespace Onyx

namespace Onyx::Resources
{
// handles are re-used, so at some point generation tracking will be needed

inline u32 GetResourceTypeAsInteger(const Handle handle)
{
    return (handle & ONYX_RESOURCE_TYPE_MASK) >> ONYX_RESOURCE_TYPE_SHIFT;
}

inline ResourceType GetResourceType(const Handle handle)
{
    return ResourceType(GetResourceTypeAsInteger(handle));
}

inline bool IsResourceNull(const Resource handle)
{
    return (handle & ONYX_RESOURCE_ID_MASK) == NullResource;
}
inline bool IsResourcePoolNull(const Handle handle)
{
    return (handle & ONYX_RESOURCE_POOL_ID_MASK) == NullResourcePool;
}

// this one is a bit niche
inline bool IsResourcePoolIdNull(const u32 poolId)
{
    return ((poolId << ONYX_RESOURCE_POOL_SHIFT) & ONYX_RESOURCE_POOL_ID_MASK) == NullResourcePool;
}

inline u32 GetResourceId(const Resource handle)
{
    return handle & ONYX_RESOURCE_ID_MASK;
}
inline u32 GetResourcePoolId(const Handle handle)
{
    return (handle & ONYX_RESOURCE_POOL_ID_MASK) >> ONYX_RESOURCE_POOL_SHIFT;
}

inline ResourcePool GetResourcePool(const Resource handle)
{
    return (handle & ONYX_RESOURCE_POOL_MASK) | NullResource;
}

inline Resource CreateResourceHandle(const ResourceType rtype, const u32 resourceId,
                                     const u32 poolId = ONYX_MAX_RESOURCE_POOLS)
{
    TKIT_ASSERT(rtype < Resource_Count,
                "[ONYX][RESOURCES] Cannot create a resource handle with an invalid resource type");
    TKIT_ASSERT(resourceId <= ONYX_MAX_RESOURCE_IDS,
                "[ONYX][RESOURCES] Cannot create a resource handle with a resource id ({:#010x}) "
                "that exceeds the maximum bits allocated "
                "for it, as it would break the handle",
                resourceId);
    TKIT_ASSERT(
        poolId <= ONYX_MAX_RESOURCE_POOLS,
        "[ONYX][RESOURCES] Cannot create a resource handle with a pool id ({:#010x}) that exceeds the maximum bits "
        "allocated "
        "for it, as it would break the handle",
        poolId);
    return (u32(rtype) << ONYX_RESOURCE_TYPE_SHIFT) | (poolId << ONYX_RESOURCE_POOL_SHIFT) | resourceId;
}

inline ResourcePool CreateResourcePoolHandle(const ResourceType rtype, const u32 poolId)
{
    TKIT_ASSERT(rtype < Resource_PoolCount,
                "[ONYX][RESOURCES] Cannot create a resource handle with an invalid resource type");
    TKIT_ASSERT(
        poolId <= ONYX_MAX_RESOURCE_POOLS,
        "[ONYX][RESOURCES] Cannot create a resource handle with a pool id ({:#010x}) that exceeds the maximum bits "
        "allocated "
        "for it, as it would break the handle",
        poolId);
    return (u32(rtype) << ONYX_RESOURCE_TYPE_SHIFT) | (poolId << ONYX_RESOURCE_POOL_SHIFT) | NullResource;
}

} // namespace Onyx::Resources
