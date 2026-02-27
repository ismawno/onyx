#include "onyx/core/pch.hpp"
#include "onyx/property/instance.hpp"

namespace Onyx
{
const char *ToString(Geometry geo)
{
    switch (geo)
    {
    case Geometry_Circle:
        return "Geometry_Circle";
    case Geometry_StaticMesh:
        return "Geometry_StaticMesh";
    default:
        return "Unknown";
    }
}

const char *ToString(LightType light)
{
    switch (light)
    {
    case Light_Point:
        return "Light_Point";
    case Light_Directional:
        return "Light_Directional";
    default:
        return "Unknown";
    }
}

const char *ToString(StencilPass pass)
{
    switch (pass)
    {
    case StencilPass_NoStencilWriteDoFill:
        return "StencilPass_NoStencilWriteDoFill";
    case StencilPass_DoStencilWriteDoFill:
        return "StencilPass_DoStencilWriteDoFill";
    case StencilPass_DoStencilWriteNoFill:
        return "StencilPass_DoStencilWriteNoFill";
    case StencilPass_DoStencilTestNoFill:
        return "StencilPass_DoStencilTestNoFill";
    default:
        return "Unknown";
    }
}

const char *ToString(DrawPass pass)
{
    switch (pass)
    {
    case DrawPass_Fill:
        return "DrawPass_Fill";
    case DrawPass_Outline:
        return "DrawPass_Outline";
    default:
        return "Unknown";
    }
}

const char *ToString(Shading shading)
{
    switch (shading)
    {
    case Shading_Unlit:
        return "Shading_Unlit";
    case Shading_Lit:
        return "Shading_Lit";
    default:
        return "Unknown";
    }
}
} // namespace Onyx
