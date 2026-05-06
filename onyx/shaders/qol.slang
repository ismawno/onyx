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

typedef float32_t2x3 f32m2x3;

typedef u32 ViewMask;

#include "../include/onyx/core/definitions.hpp"

static constexpr u32 NullResource = ONYX_RESOURCE_ID_MASK;


