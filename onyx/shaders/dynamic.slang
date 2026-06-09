#pragma once

#include "common.slang"

typedef StructuredBuffer<DynamicInstanceData2D, Std430DataLayout> DynamicInstances2D;
typedef StructuredBuffer<DynamicInstanceData3D, Std430DataLayout> DynamicInstances3D;

typedef InstanceData2D DynamicInstanceData2D;
typedef InstanceData3D DynamicInstanceData3D;

