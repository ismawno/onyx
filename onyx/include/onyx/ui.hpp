#pragma once

#include "onyx/handle.hpp"
#include "onyx/color.hpp"
#include "onyx/instance.hpp"
#include "onyx/pass.hpp"
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
    LayoutDirection_LeftToRight,
    LayoutDirection_RightToLeft,
    LayoutDirection_BottomToTop,
    LayoutDirection_TopToBottom,
};

enum LayoutAxis : u8
{
    LayoutAxis_Horizontal,
    LayoutAxis_Vertical
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
    LayoutShape_Rectangle,
    LayoutShape_RoundedRectangle,
    LayoutShape_Custom
};

struct LayoutShape
{
    Resource Handle;
    f32 Radius;
    LayoutShapeType Type;

    static constexpr LayoutShape Circle()
    {
        return {NullHandle, 0.f, LayoutShape_Circle};
    }
    static constexpr LayoutShape Rectangle(const Resource handle = NullHandle)
    {
        return {handle, 0.f, LayoutShape_Rectangle};
    }
    static constexpr LayoutShape Rectangle(const f32 radius, const Resource handle = NullHandle)
    {
        return {handle, radius, TKit::ApproachesZero(radius) ? LayoutShape_Rectangle : LayoutShape_RoundedRectangle};
    }
    static constexpr LayoutShape Custom(const Resource handle)
    {
        return {handle, 0.f, LayoutShape_Custom};
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

enum LayoutOverflowMode : u8
{
    LayoutOverflow_Spill,
    LayoutOverflow_Clip,
};

struct LayoutElement
{
    LayoutShape Shape;
    f32v2 Position{0.f};
    f32v2 Size;
    f32v2 MinSize;
    f32v2 MaxSize;
    f32v2 ClipMin;
    f32v2 ClipMax;
    f32v2 ChildOffset;
    f32v2 SelfOffset;
    vec2<LayoutSizingType> Sizing;

    f32v4 Padding;

    u32 Parent;
    Resource Font;
    Resource Material;
    TKit::String Text;
    TKit::TierArray<u32> Children{};

    f32 CornerRadius;
    f32 ChildGap;
    f32 FontSize;
    f32 OutlineWidth;

    Color FillColor;
    Color OutlineColor;
    vec2<Alignment> SelfAlignment{Alignment_Bottom};
    vec2<Alignment> ChildAlignment{Alignment_Bottom};
    LayoutDirection Direction;
    LayoutElementType Type;
    LayoutTextMode TextMode;
    LayoutOverflowMode SelfOverflow;
    LayoutOverflowMode ChildOverflow;
};

struct LayoutElementInfo
{
    TKit::String Text;
    f32v2 Position;
    f32v2 Size;
    f32v2 ClipMin;
    f32v2 ClipMax;
    Color FillColor;
    Color OutlineColor;
    f32 Radius;
    f32 OutlineWidth;
    Resource Handle;
    Resource Material;
    Geometry Geo;
    vec2<Alignment> Alignment;
    LayoutShapeType ShapeType;
    RenderModeFlags RenderFlags;
};

// TODO(Isma): Add floating panels
// TODO(Isma): Implement ids and hashing
struct LayoutPanelParameters
{
    Color FillColor = Color_Transparent;
    Color OutlineColor = Color_White;
    LayoutDirection Direction = LayoutDirection_LeftToRight;
    vec2<Alignment> Alignment{Alignment_Canonical};
    vec2<LayoutSizing> Sizing{LayoutSizing::Fit()};
    LayoutShape Shape = LayoutShape::Rectangle();
    Resource Material = NullHandle;
    // NOTE(Isma): Could add overflow mode override per-children (as in a ChildOverflow and SelfOverflow parameters).
    // Skipping for now
    LayoutOverflowMode Overflow = LayoutOverflow_Clip;
    f32v2 ChildOffset{0.f};
    f32v2 SelfOffset{0.f};
    f32v4 Padding{0.f};
    f32 ChildGap = 0.f;
    f32 OutlineWidth = 0.f;
};

struct LayoutTextParameters
{
    Color FillColor = Color_White;
    Color OutlineColor = Color_Transparent;
    LayoutTextMode Mode = TextMode_Unbounded;
    f32 FontSize = 1.f;
    f32 OutlineWidth = 0.25f;
    f32v2 Offset{0.f};
    Resource Font = NullHandle;
    Resource Material = NullHandle;
};

struct LayoutSpecs
{
    Resource RectangleMesh = NullHandle;
    Resource RoundedRectangleMesh = NullHandle;
    Resource Font = NullHandle;
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
    void fitPass(LayoutAxis axis);
    void growShrinkPass(LayoutAxis axis);
    void wrapText();
    void positionPass();
    LayoutAxis getAxis(const LayoutDirection dir)
    {
        return LayoutAxis(dir >> 1);
    }

    TKit::TierArray<LayoutElement> m_Elements{};
    TKit::TierArray<u32> m_Stack{};
    TKit::TierArray<u32> m_Breadth{};
    TKit::TierArray<u32> m_ReversedBreadth{};
    TKit::TierArray<LayoutElementInfo> m_ElementInfo{};

    LayoutSpecs m_Specs{};
};
} // namespace Onyx
