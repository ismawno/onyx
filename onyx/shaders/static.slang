
#pragma once

#include "common.slang"

typedef StructuredBuffer<StaticInstanceData2D, Std430DataLayout> StaticInstances2D;
typedef StructuredBuffer<StaticInstanceData3D, Std430DataLayout> StaticInstances3D;

struct StaticInstanceData2D
{
    InstanceData2D Data;
    u32 BoundsHandle;
}
struct StaticInstanceData3D
{
    InstanceData3D Data;
    u32 BoundsHandle;
}
