#pragma once

#include "onyx/core/alias.hpp"
#include "tkit/utils/limits.hpp"
#include "tkit/utils/debug.hpp"

#define ONYX_ASSET_TYPE_BITS 3U
#define ONYX_ASSET_POOL_BITS 8U

#define ONYX_ASSET_TYPE_SHIFT (32U - ONYX_ASSET_TYPE_BITS)
#define ONYX_ASSET_POOL_SHIFT (32U - ONYX_ASSET_TYPE_BITS - ONYX_ASSET_POOL_BITS)

#define ONYX_ASSET_BITS ONYX_ASSET_POOL_SHIFT

#define ONYX_ASSET_TYPE_MASK (0xFFFFFFFFU << ONYX_ASSET_TYPE_SHIFT)
#define ONYX_ASSET_POOL_MASK (0xFFFFFFFFU << ONYX_ASSET_POOL_SHIFT)
#define ONYX_ASSET_POOL_ID_MASK (ONYX_ASSET_POOL_MASK & ~ONYX_ASSET_TYPE_MASK)
#define ONYX_ASSET_ID_MASK ~ONYX_ASSET_POOL_MASK

#define ONYX_MAX_ASSET_POOLS ((1U << ONYX_ASSET_POOL_BITS) - 1U)
#define ONYX_MAX_ASSETS ((1U << ONYX_ASSET_BITS) - 1U) // per pool

#ifdef TKIT_ENABLE_ASSERTS
#    define ONYX_CHECK_HANDLE_HAS_VALID_ASSET_TYPE(hndl)                                                               \
        TKIT_ASSERT(Onyx::Assets::GetAssetTypeAsInteger(hndl) < Onyx::Asset_Count,                                     \
                    "[ONYX][ASSETS] The handle {:#010x} does not match any known asset types ({}), which likely "      \
                    "means it is a "                                                                                   \
                    "broken handle",                                                                                   \
                    hndl, Onyx::Assets::GetAssetTypeAsInteger(hndl))

#    define ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(hndl)                                                          \
        ONYX_CHECK_HANDLE_HAS_VALID_ASSET_TYPE(hndl);                                                                  \
        TKIT_ASSERT(                                                                                                   \
            Onyx::Assets::GetAssetTypeAsInteger(hndl) < Onyx::Asset_PoolCount,                                         \
            "[ONYX][ASSETS] The handle {:#010x} is a '{}' handle, which does not have any asset pool associated",      \
            hndl, Onyx::ToString(Onyx::Assets::GetAssetType(hndl)))

#    define __ONYX_CHECK_HANDLE_HAS_ASSET_TYPE(hndl, atype)                                                            \
        TKIT_ASSERT(Onyx::Assets::GetAssetType(hndl) == atype,                                                         \
                    "[ONYX][ASSETS] The handle {:#010x} is not a '{}' handle, but rather a '{}' handle", hndl,         \
                    Onyx::ToString(atype), Onyx::ToString(Onyx::Assets::GetAssetType(hndl)))

#    define ONYX_CHECK_HANDLE_HAS_ASSET_TYPE(hndl, atype)                                                              \
        ONYX_CHECK_HANDLE_HAS_VALID_ASSET_TYPE(hndl);                                                                  \
        __ONYX_CHECK_HANDLE_HAS_ASSET_TYPE(hndl, atype)

#    define ONYX_CHECK_HANDLE_HAS_ASSET_POOL_TYPE(hndl, atype)                                                         \
        ONYX_CHECK_HANDLE_HAS_VALID_ASSET_POOL_TYPE(hndl);                                                             \
        __ONYX_CHECK_HANDLE_HAS_ASSET_TYPE(hndl, atype)

#    define ONYX_CHECK_ASSET_IS_NOT_NULL(hndl)                                                                         \
        TKIT_ASSERT(!Onyx::Assets::IsAssetNull(hndl), "[ONYX][ASSETS] The handle {:#010x} has a null asset id", hndl)

#    define ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(hndl)                                                                    \
        TKIT_ASSERT(!Onyx::Assets::IsAssetPoolNull(hndl),                                                              \
                    "[ONYX][ASSETS] The handle {:#010x} has a null asset pool id", hndl)

#    define ONYX_CHECK_ASSET_POOL_ID_IS_NOT_NULL(hndl)                                                                 \
        TKIT_ASSERT(!Onyx::Assets::IsAssetPoolIdNull(hndl),                                                            \
                    "[ONYX][ASSETS] The handle {:#010x} has a null asset pool id", hndl)

#    define ONYX_CHECK_ASSET_IS_VALID(hndl, atype)                                                                     \
        ONYX_CHECK_HANDLE_HAS_ASSET_TYPE(hndl, atype);                                                                 \
        TKIT_ASSERT(Onyx::Assets::IsAssetValid(hndl, atype),                                                           \
                    "[ONYX][ASSETS] The handle {:#010x} is not a valid '{}' handle", hndl, ToString(atype))

#    define ONYX_CHECK_ASSET_IS_VALID_WITH_DIM(hndl, atype, dim)                                                       \
        ONYX_CHECK_HANDLE_HAS_ASSET_TYPE(hndl, atype);                                                                 \
        TKIT_ASSERT(Onyx::Assets::IsAssetValid<dim>(hndl, atype),                                                      \
                    "[ONYX][ASSETS] The handle {:#010x} is not a valid '{}' handle", hndl, ToString(atype))

#    define ONYX_CHECK_ASSET_POOL_IS_VALID(hndl, ptype)                                                                \
        ONYX_CHECK_HANDLE_HAS_ASSET_POOL_TYPE(hndl, ptype);                                                            \
        TKIT_ASSERT(Onyx::Assets::IsAssetPoolValid(hndl, ptype),                                                       \
                    "[ONYX][ASSETS] The handle {:#010x} does not contain a valid '{}' pool handle", hndl,              \
                    ToString(ptype))

#    define ONYX_CHECK_ASSET_POOL_IS_VALID_WITH_DIM(hndl, ptype, dim)                                                  \
        ONYX_CHECK_HANDLE_HAS_ASSET_POOL_TYPE(hndl, ptype);                                                            \
        TKIT_ASSERT(Onyx::Assets::IsAssetPoolValid<dim>(hndl, ptype),                                                  \
                    "[ONYX][ASSETS] The handle {:#010x} does not contain a valid '{}' pool handle", hndl,              \
                    ToString(ptype))

#else
#    define ONYX_CHECK_HAS_VALID_ASSET_TYPE(...)
#    define ONYX_CHECK_HAS_VALID_ASSET_POOL_TYPE(...)
#    define ONYX_CHECK_HAS_ASSET_TYPE(...)
#    define ONYX_CHECK_HAS_ASSET_POOL_TYPE(...)
#    define ONYX_CHECK_ASSET_IS_NOT_NULL(...)
#    define ONYX_CHECK_ASSET_POOL_IS_NOT_NULL(...)
#    define ONYX_CHECK_ASSET_POOL_ID_IS_NOT_NULL(...)
#    define ONYX_CHECK_ASSET_IS_VALID(...)
#    define ONYX_CHECK_ASSET_IS_VALID_WITH_DIM(...)
#    define ONYX_CHECK_ASSET_POOL_IS_VALID(...)
#    define ONYX_CHECK_ASSET_POOL_IS_VALID_WITH_DIM(...)
#endif

namespace Onyx
{
using Handle = u32;
constexpr Handle NullHandle = TKit::Limits<Handle>::Max();

using Asset = Handle;
constexpr Asset NullAsset = ONYX_ASSET_ID_MASK;

using AssetPool = Handle;
constexpr AssetPool NullAssetPool = ONYX_ASSET_POOL_ID_MASK;

enum AssetType : u8
{
    Asset_StaticMesh,
    Asset_ParametricMesh,
    Asset_GlyphMesh,
    Asset_Font,
    Asset_Material,
    Asset_Bounds,
    Asset_Sampler,
    Asset_Texture,
    Asset_Count,

    // not ideal to have them here but idc too much
    Asset_MeshCount = Asset_Font,
    Asset_PoolCount = Asset_Material
};

const char *ToString(AssetType atype);

} // namespace Onyx

namespace Onyx::Assets
{

// handles are re-used, so at some point generation tracking will be needed

inline u32 GetAssetTypeAsInteger(const Handle handle)
{
    return (handle & ONYX_ASSET_TYPE_MASK) >> ONYX_ASSET_TYPE_SHIFT;
}

inline AssetType GetAssetType(const Handle handle)
{
    return AssetType(GetAssetTypeAsInteger(handle));
}

inline bool IsAssetNull(const Asset handle)
{
    return (handle & ONYX_ASSET_ID_MASK) == NullAsset;
}
inline bool IsAssetPoolNull(const Handle handle)
{
    return (handle & ONYX_ASSET_POOL_ID_MASK) == NullAssetPool;
}

// this one is a bit niche
inline bool IsAssetPoolIdNull(const u32 poolId)
{
    return ((poolId << ONYX_ASSET_POOL_SHIFT) & ONYX_ASSET_POOL_ID_MASK) == NullAssetPool;
}

inline u32 GetAssetId(const Asset handle)
{
    return handle & ONYX_ASSET_ID_MASK;
}
inline u32 GetAssetPoolId(const Handle handle)
{
    return (handle & ONYX_ASSET_POOL_ID_MASK) >> ONYX_ASSET_POOL_SHIFT;
}

inline AssetPool GetAssetPool(const Asset handle)
{
    return (handle & ONYX_ASSET_POOL_MASK) | NullAsset;
}

inline Asset CreateAssetHandle(const AssetType atype, const u32 assetId, const u32 poolId = ONYX_MAX_ASSET_POOLS)
{
    TKIT_ASSERT(atype < Asset_Count, "[ONYX][ASSETS] Cannot create an asset handle with an invalid asset type");
    TKIT_ASSERT(assetId <= ONYX_MAX_ASSETS,
                "[ONYX][ASSETS] Cannot create an asset handle with an asset id ({:#010x}) "
                "that exceeds the maximum bits allocated "
                "for it, as it would break the handle",
                assetId);
    TKIT_ASSERT(poolId <= ONYX_MAX_ASSET_POOLS,
                "[ONYX][ASSETS] Cannot create an asset handle with a pool id ({:#010x}) that exceeds the maximum bits "
                "allocated "
                "for it, as it would break the handle",
                poolId);
    return (u32(atype) << ONYX_ASSET_TYPE_SHIFT) | (poolId << ONYX_ASSET_POOL_SHIFT) | assetId;
}

inline AssetPool CreateAssetPoolHandle(const AssetType atype, const u32 poolId)
{
    TKIT_ASSERT(atype < Asset_PoolCount, "[ONYX][ASSETS] Cannot create an asset handle with an invalid asset type");
    TKIT_ASSERT(poolId <= ONYX_MAX_ASSET_POOLS,
                "[ONYX][ASSETS] Cannot create an asset handle with a pool id ({:#010x}) that exceeds the maximum bits "
                "allocated "
                "for it, as it would break the handle",
                poolId);
    return (u32(atype) << ONYX_ASSET_TYPE_SHIFT) | (poolId << ONYX_ASSET_POOL_SHIFT) | NullAsset;
}

} // namespace Onyx::Assets
