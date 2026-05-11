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

struct LayoutElement
{
    f32v2 Position{0.f};
    f32v2 Size;
    f32v2 MinSize;
    f32v2 MaxSize;
    vec2<LayoutSizingType> Sizing;

    f32v4 Padding;

    u32 Parent;
    TKit::TierArray<u32> Children{};

    f32 CornerRadius;
    f32 ChildGap;

    Color Color;
    LayoutDirection Direction;
    vec2<Alignment> SelfAlignment{Alignment_Bottom};
    vec2<Alignment> ChildAlignment{Alignment_Bottom};
};

struct LayoutElementInfo
{
    f32v2 Position;
    f32v2 Size;
    f32 Radius;
    Color Color;
    Resource Handle;
    Geometry Geo;
    vec2<Alignment> Alignment;
};

struct PanelParameters
{
    Color Color = Color_Transparent;
    LayoutDirection Direction = LayoutDirection_Horizontal;
    vec2<Alignment> Alignment{Alignment_Bottom};
    vec2<LayoutSizing> Sizing{LayoutSizing::Fit()};
    f32v4 Padding{0.f};
    f32 ChildGap = 0.f;
    f32 CornerRadius = 0.f;
};

class Layout
{
  public:
    Layout(const Resource square, const Resource roundedSquare) : m_Square(square), m_RoundedSquare(roundedSquare)
    {
    }

    void BeginPanel(const PanelParameters &params);
    void EndPanel();

    void Compile();

    const TKit::TierArray<LayoutElementInfo> &GetElementsInfo() const
    {
        return m_ElementInfo;
    }

  private:
    void fitPass(u32 axis);
    void growShrinkPass(u32 axis);
    void positionPass();

    TKit::TierArray<LayoutElement> m_Elements{};
    TKit::TierArray<u32> m_Stack{};
    TKit::TierArray<u32> m_Breadth{};
    TKit::TierArray<u32> m_ReversedBreadth{};
    TKit::TierArray<LayoutElementInfo> m_ElementInfo{};

    Resource m_Square;
    Resource m_RoundedSquare;
};
} // namespace Onyx
