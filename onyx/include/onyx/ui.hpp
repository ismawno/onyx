#pragma once

#include "onyx/handle.hpp"
#include "onyx/color.hpp"
#include "onyx/instance.hpp"
#include "tkit/container/tier_array.hpp"

namespace Onyx
{
enum LayoutSizingType : u8
{
    LayoutSizing_Fixed,
    LayoutSizing_Fit,
    LayoutSizing_Grow,
    LayoutSizing_None,
};

enum LayoutDirection : u8
{
    LayoutDirection_Horizontal,
    LayoutDirection_Vertical,
};

struct LayoutSizing
{
    f32 Size;
    f32 Min;
    f32 Max;
    LayoutSizingType Type;

    static constexpr LayoutSizing Fixed(const f32 size)
    {
        return {size, 0.f, TKIT_F32_MAX, LayoutSizing_Fixed};
    }
    static constexpr LayoutSizing Fit(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return {0.f, min, max, LayoutSizing_Fit};
    }
    static constexpr LayoutSizing Grow(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return {0.f, min, max, LayoutSizing_Grow};
    }
};

enum LayoutShapeType : u8
{
    LayoutShape_Circle,
    LayoutShape_Triangle,
    LayoutShape_Rectangle,
    LayoutShape_Stadium,
    LayoutShape_RoundedRectangle
};

struct LayoutShape
{
    Resource Handle;
    f32 Rotation;
    f32 Radius;
    LayoutShapeType Type;

    static constexpr LayoutShape Circle(const Resource handle = NullHandle)
    {
        return {handle, 0.f, 0.f, LayoutShape_Circle};
    }
    static constexpr LayoutShape Triangle(const Resource handle = NullHandle)
    {
        return {handle, 0.f, 0.f, LayoutShape_Triangle};
    }
    static constexpr LayoutShape Triangle(const f32 rotation, const Resource handle = NullHandle)
    {
        return {handle, rotation, 0.f, LayoutShape_Triangle};
    }
    static constexpr LayoutShape Rectangle(const Resource handle = NullHandle)
    {
        return {handle, 0.f, 0.f, LayoutShape_Rectangle};
    }
    static constexpr LayoutShape Rectangle(const f32 radius, const Resource handle = NullHandle)
    {
        return {handle, 0.f, radius,
                TKit::ApproachesZero(radius) ? LayoutShape_Rectangle : LayoutShape_RoundedRectangle};
    }
    static constexpr LayoutShape Rectangle(const f32 radius, const f32 rotation, const Resource handle = NullHandle)
    {
        return {handle, rotation, radius,
                TKit::ApproachesZero(radius) ? LayoutShape_Rectangle : LayoutShape_RoundedRectangle};
    }
    static constexpr LayoutShape Stadium(const Resource handle = NullHandle)
    {
        return {handle, 0.f, 0.f, LayoutShape_Stadium};
    }
    static constexpr LayoutShape Stadium(const f32 rotation, const Resource handle = NullHandle)
    {
        return {handle, rotation, 0.f, LayoutShape_Stadium};
    }
};

enum LayoutElementType : u8
{
    LayoutElement_Panel,
    LayoutElement_Text
};

enum LayoutTextMode : u8
{
    TextMode_Unbounded,
    TextMode_Wrapped
};

struct LayoutElement
{
    LayoutShape Shape;
    f32v2 Position{0.f};
    f32v2 Size;
    f32v2 MinSize;
    f32v2 MaxSize;
    f32v2 ChildOffset;
    f32v2 SelfOffset;
    vec2<LayoutSizingType> Sizing;

    f32v4 Padding;

    u32 Parent;
    Resource Font;
    TKit::String Text;
    TKit::TierArray<u32> Children{};

    f32 CornerRadius;
    f32 ChildGap;
    f32 FontSize;

    Color Color;
    vec2<Alignment> SelfAlignment{Alignment_Bottom};
    vec2<Alignment> ChildAlignment{Alignment_Bottom};
    LayoutDirection Direction;
    LayoutElementType Type;
    LayoutTextMode TextMode;
};

struct LayoutElementInfo
{
    TKit::String Text;
    LayoutShapeType ShapeType;
    f32v2 Position;
    f32v2 Size;
    f32 Radius;
    f32 Rotation;
    Color Color;
    Resource Handle;
    Geometry Geo;
    vec2<Alignment> Alignment;
};

// TODO(Isma): Add floating panels
// TODO(Isma): Add clip mode
// TODO(Isma): Add outlines
// TODO(Isma): Implement ids and hashing
struct LayoutPanelParameters
{
    Color Color = Color_Transparent;
    LayoutDirection Direction = LayoutDirection_Horizontal;
    vec2<Alignment> Alignment{Alignment_Bottom};
    vec2<LayoutSizing> Sizing{LayoutSizing::Fit()};
    LayoutShape Shape = LayoutShape::Rectangle();
    f32v2 ChildOffset{0.f};
    f32v2 SelfOffset{0.f};
    f32v4 Padding{0.f};
    f32 ChildGap = 0.f;
};

struct LayoutTextParameters
{
    Color Color = Color_White;
    LayoutTextMode Mode = TextMode_Unbounded;
    f32 FontSize = 1.f;
    f32v2 Offset{0.f};
    Resource Font = NullHandle;
};

struct LayoutSpecs
{
    Resource TriangleMesh = NullHandle;
    Resource RectangleMesh = NullHandle;
    Resource StadiumMesh = NullHandle;
    Resource RoundedRectangleMesh = NullHandle;
};

class Layout
{
  public:
    Layout(const LayoutSpecs &specs = {}) : m_Specs(specs)
    {
    }

    void BeginPanel(const LayoutPanelParameters &params);
    void EndPanel();

    void Text(TKit::StringView text, const LayoutTextParameters &params = {});

    void Compile();

    const TKit::TierArray<LayoutElementInfo> &GetElementsInfo() const
    {
        return m_ElementInfo;
    }

  private:
    void fitPass(u32 axis);
    void growShrinkPass(u32 axis);
    void wrapText();
    void positionPass();

    TKit::TierArray<LayoutElement> m_Elements{};
    TKit::TierArray<u32> m_Stack{};
    TKit::TierArray<u32> m_Breadth{};
    TKit::TierArray<u32> m_ReversedBreadth{};
    TKit::TierArray<LayoutElementInfo> m_ElementInfo{};

    LayoutSpecs m_Specs{};
};
} // namespace Onyx
