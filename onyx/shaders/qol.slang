#pragma once

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float16_t f16;
typedef float32_t f32;
typedef float64_t f64;

typedef float32_t2 f32v2;
typedef float32_t3 f32v3;
typedef float32_t4 f32v4;

typedef uint32_t2 u32v2;
typedef uint32_t3 u32v3;
typedef uint32_t4 u32v4;

typedef int32_t2 i32v2;
typedef int32_t3 i32v3;
typedef int32_t4 i32v4;

typedef float32_t2x2 f32m2;
typedef float32_t3x3 f32m3;
typedef float32_t4x4 f32m4;

typedef u32 ViewMask;

#define ONYX_ASSET_TYPE_BITS 3U
#define ONYX_ASSET_POOL_BITS 8U

#define ONYX_ASSET_TYPE_SHIFT (32U - ONYX_ASSET_TYPE_BITS)
#define ONYX_ASSET_POOL_SHIFT (32U - ONYX_ASSET_TYPE_BITS - ONYX_ASSET_POOL_BITS)

#define ONYX_ASSET_TYPE_MASK (0xFFFFFFFFU << ONYX_ASSET_TYPE_SHIFT)
#define ONYX_ASSET_POOL_MASK (0xFFFFFFFFU << ONYX_ASSET_POOL_SHIFT)

#define ONYX_ASSET_ID_MASK ~ONYX_ASSET_POOL_MASK
#define ONYX_ASSET_POOL_ID_MASK (ONYX_ASSET_POOL_MASK & ~ONYX_ASSET_TYPE_MASK)

#define ONYX_MAX_SAMPLERS 8
#define ONYX_MAX_TEXTURES 1024

#define ONYX_INSTANCES_BINDING 0

#define ONYX_BOUNDS2_BINDING 3
#define ONYX_BOUNDS3_BINDING 3
#define ONYX_BOUNDS3_STENCIL_BINDING 4

#define ONYX_SAMPLERS_BINDING 1

#define ONYX_TEXTURES_BINDING 2

#define ONYX_MATERIALS_BINDING 4

#define ONYX_PLIGHTS_BINDING 5
#define ONYX_DLIGHTS_BINDING 9

#define ONYX_SHADOW_SAMPLER_BINDING 6
#define ONYX_SHADOW_MAPS_BINDING 7
#define ONYX_POINT_MAPS_BINDING 7
#define ONYX_DIRECTIONAL_MAPS_BINDING 8

#define ONYX_OCCLUSION_MAP_BINDING 0
#define ONYX_DISTANCE_MAP_BINDING 1

#define ONYX_MAX_TEXTURE_MAPS 32
#define ONYX_MAX_CASCADES 4

static constexpr u32 NullAsset = ONYX_ASSET_ID_MASK;


