#pragma once

#include "onyx/handle.hpp"
#include "onyx/color.hpp"
#include "onyx/instance.hpp"
#include "onyx/pass.hpp"
#include "onyx/font.hpp"
#include "tkit/container/tier_array.hpp"
#include "tkit/container/hash_map.hpp"
#include "tkit/utils/hash.hpp"

namespace Onyx
{
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

enum LayoutSizingType : u8
{
    LayoutSizing_Absolute,
    LayoutSizing_Normalized,
    LayoutSizing_Fit,
    LayoutSizing_Grow,
    LayoutSizing_Flex,
};
struct LayoutSizing
{
    f32 Size;
    f32 Min;
    f32 Max;
    LayoutSizingType Type;

    static constexpr LayoutSizing Absolute(const f32 size)
    {
        return {size, 0.f, TKIT_F32_MAX, LayoutSizing_Absolute};
    }
    static constexpr LayoutSizing Normalized(const f32 size)
    {
        return {size, 0.f, TKIT_F32_MAX, LayoutSizing_Normalized};
    }
    static constexpr LayoutSizing Fit(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return {0.f, min, max, LayoutSizing_Fit};
    }
    static constexpr LayoutSizing Grow(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return {0.f, min, max, LayoutSizing_Grow};
    }
    static constexpr LayoutSizing Flex(const f32 min = 0.f, const f32 max = TKIT_F32_MAX)
    {
        return {0.f, min, max, LayoutSizing_Flex};
    }

    static constexpr vec2<LayoutSizing> Absolute(const f32v2 &size)
    {
        return {Absolute(size[0]), Absolute(size[1])};
    }
    static constexpr vec2<LayoutSizing> Normalized(const f32v2 &size)
    {
        return {Normalized(size[0]), Normalized(size[1])};
    }
    static constexpr vec2<LayoutSizing> Fit(const f32v2 &mn, const f32v2 &mx = f32v2{TKIT_F32_MAX})
    {
        return {Fit(mn[0], mx[0]), Fit(mn[1], mx[1])};
    }
    static constexpr vec2<LayoutSizing> Grow(const f32v2 &mn, const f32v2 &mx = f32v2{TKIT_F32_MAX})
    {
        return {Grow(mn[0], mx[0]), Grow(mn[1], mx[1])};
    }
};

enum LayoutOffsetType : u8
{
    LayoutOffset_Absolute,
    LayoutOffset_Normalized,
    LayoutOffset_Relative
};

struct LayoutOffset
{
    f32 Offset;
    LayoutOffsetType Type;

    static constexpr LayoutOffset Absolute(const f32 offset)
    {
        return {offset, LayoutOffset_Absolute};
    }
    static constexpr LayoutOffset Normalized(const f32 offset)
    {
        return {offset, LayoutOffset_Normalized};
    }
    static constexpr LayoutOffset Relative(const f32 offset)
    {
        return {offset, LayoutOffset_Relative};
    }
    static constexpr vec2<LayoutOffset> Absolute(const f32v2 &offset)
    {
        return {Absolute(offset[0]), Absolute(offset[1])};
    }
    static constexpr vec2<LayoutOffset> Normalized(const f32v2 &offset)
    {
        return {Normalized(offset[0]), Normalized(offset[1])};
    }
    static constexpr vec2<LayoutOffset> Relative(const f32v2 &offset)
    {
        return {Relative(offset[0]), Relative(offset[1])};
    }
};

enum LayoutShapeType : u8
{
    LayoutShape_Circle,
    LayoutShape_Rectangle,
    // NOTE(Isma): Could support stadiums as well for scrollbars instead of relying on "degenerate" rounded rect state
    // where width or height is zero
    LayoutShape_RoundedRectangle,
    LayoutShape_Glyph,
    LayoutShape_Text,
    LayoutShape_Unicode,
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
        return {handle, radius, Math::ApproachesZero(radius) ? LayoutShape_Rectangle : LayoutShape_RoundedRectangle};
    }
    static constexpr LayoutShape Glyph(const Resource handle)
    {
        return {handle, 0.f, LayoutShape_Glyph};
    }
    static constexpr LayoutShape Custom(const Resource handle)
    {
        return {handle, 0.f, LayoutShape_Custom};
    }
};

enum LayoutElementType : u8
{
    LayoutElement_Panel,
    LayoutElement_Floating,
    LayoutElement_Text,
    LayoutElement_Unicode,
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

enum LayoutAttachment : u8
{
    LayoutAttachment_Canonical,
    LayoutAttachment_Center,
    LayoutAttachment_Mirrored,
    LayoutAttachment_Left = LayoutAttachment_Canonical,
    LayoutAttachment_Right = LayoutAttachment_Mirrored,
    LayoutAttachment_Bottom = LayoutAttachment_Canonical,
    LayoutAttachment_Top = LayoutAttachment_Mirrored,
};

struct LayoutFloatingParameters
{
    // NOTE(Isma): Should maybe use flags here? 3 bools already
    bool Enable = false;
    bool DrawOnTop = true;
    bool Clip = false;
    vec2<LayoutAttachment> Attachment{LayoutAttachment_Canonical};
    vec2<Alignment> Alignment{Alignment_Canonical};
};

// TODO(Isma): Add here helpers such as ShouldParticipateInFit etc, for when we add custom rendering to layouts
struct LayoutElement
{
    usz Id;
    LayoutShape Shape;
    f32v2 Position{0.f};
    f32v2 Size;
    f32v2 MinSize;
    f32v2 MaxSize;
    f32v2 ClipMin;
    f32v2 ClipMax;
    f32v2 ChildOffset;
    f32v2 SelfOffset;
    f32v2 ShrinkTolerance;
    f32v2 ChildrenSize;

    vec2<LayoutSizingType> Sizing;
    vec2<LayoutOffsetType> ChildOffsetType;
    vec2<LayoutOffsetType> SelfOffsetType;
    vec2<LayoutAttachment> FloatAttachment;
    vec2<Alignment> FloatAlignment;
    bool FloatClip;
    bool DrawOnTop;

    f32v4 Padding; // left right bottom top

    u32 Parent;
    Resource Font;
    Resource Material;
    u32 Unicode;
    TKit::String Text;
    TKit::TierArray<u32> Children{};
    u32 NonFloatChildCount = 0;

    f32 CornerRadius;
    f32 ChildGap;
    f32 FontSize;
    f32 OutlineWidth;

    Color FillColor;
    Color OutlineColor;
    vec2<Alignment> Alignment{Alignment_Canonical};
    LayoutDirection Direction;
    LayoutElementType Type;
    LayoutTextMode TextMode;
    LayoutOverflowMode SelfOverflow;
    LayoutOverflowMode ChildOverflow;

    bool IsHovered(const f32v2 &pos, const f32v2 &padding = f32v2{0.f}) const;
};

struct LayoutDrawInfo
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
    u32 Unicode;
    Resource Handle;
    Resource Material;
    LayoutShapeType ShapeType;
    RenderModeFlags RenderFlags;
};

struct LayoutPanelParameters
{
    Color FillColor = Color_Transparent;
    Color OutlineColor = Color_White;
    LayoutDirection Direction = LayoutDirection_LeftToRight;

    vec2<Alignment> Alignment{Alignment_Canonical};
    vec2<LayoutSizing> Sizing{LayoutSizing::Fit()};
    vec2<LayoutOffset> ChildOffset{LayoutOffset::Absolute(0.f)};
    vec2<LayoutOffset> SelfOffset{LayoutOffset::Absolute(0.f)};

    LayoutShape Shape = LayoutShape::Rectangle();
    LayoutFloatingParameters Floating{};
    Resource Material = NullHandle;
    // NOTE(Isma): Could add overflow mode override per-children (as in a ChildOverflow and SelfOverflow parameters).
    // Skipping for now
    LayoutOverflowMode Overflow = LayoutOverflow_Clip;
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
    f32 OutlineWidth = 0.f;
    vec2<LayoutOffset> Offset{LayoutOffset::Absolute(0.f)};
    Resource Font = NullHandle;
    Resource Material = NullHandle;
};

struct LayoutUnicodeParameters
{
    Color FillColor = Color_White;
    Color OutlineColor = Color_Transparent;
    f32 Size = 1.f;
    f32 OutlineWidth = 0.f;
    vec2<LayoutOffset> Offset{LayoutOffset::Absolute(0.f)};
    Resource Font = NullHandle;
    Resource Material = NullHandle;
};

struct LayoutSpecs
{
    vec2<Alignment> RootAlignment{Alignment_Canonical};
    Resource RectangleMesh = NullHandle;
    Resource RoundedRectangleMesh = NullHandle;
    Resource Font = NullHandle;
};

// TODO(Isma): Have a tkit macro for this ::Max()
constexpr usz NullLayoutId = TKit::Limits<usz>::Max();

class Layout
{
  public:
    Layout(const LayoutSpecs &specs = {});

    usz BeginPanel(usz label, const LayoutPanelParameters &params = {})
    {
        const usz id = beginPanel(label, params);
        PushId(label);
        return id;
    }
    usz BeginPanel(const TKit::StringView label, const LayoutPanelParameters &params = {})
    {
        return BeginPanel(TKit::Hash(label), params);
    }

    // here id is not guaranteed to be persisted accross frames, so no point in returning it
    void BeginPanel(const LayoutPanelParameters &params = {})
    {
        BeginPanel(++m_AutoLabel, params);
    }

    usz Panel(usz label, const LayoutPanelParameters &params = {})
    {
        const usz id = beginPanel(label, params);
        endPanel();
        return id;
    }
    usz Panel(const TKit::StringView label, const LayoutPanelParameters &params = {})
    {
        return Panel(TKit::Hash(label), params);
    }
    void Panel(const LayoutPanelParameters &params = {})
    {
        Panel(++m_AutoLabel, params);
    }

    void EndPanel()
    {
        PopId();
        endPanel();
    }

    usz Text(usz label, TKit::StringView text, const LayoutTextParameters &params = {});
    usz Text(const TKit::StringView text, const LayoutTextParameters &params = {})
    {
        return Text(TKit::Hash(text), text, params);
    }

    usz Unicode(usz label, CodePoint code, const LayoutUnicodeParameters &params = {});
    usz Unicode(const usz label, const TKit::StringView code, const LayoutUnicodeParameters &params = {})
    {
        return Unicode(label, DecodeUTF8(code.GetData()), params);
    }
    usz Unicode(const CodePoint code, const LayoutUnicodeParameters &params = {})
    {
        return Unicode(TKit::Hash(code), code, params);
    }
    usz Unicode(const TKit::StringView code, const LayoutUnicodeParameters &params = {})
    {
        return Unicode(DecodeUTF8(code.GetData()), params);
    }

    usz GetNextId(const usz label) const
    {
        return TKit::Hash(m_IdStack.GetBack(), label);
    }
    usz GetNextId(const TKit::StringView label) const
    {
        return GetNextId(TKit::Hash(label));
    }
    usz GetNextId(const CodePoint code) const
    {
        return GetNextId(TKit::Hash(code));
    }

    const LayoutElement &GetCurrentElement() const
    {
        return m_Elements[m_ElementStack.GetBack()];
    }

    const LayoutElement *QueryElement(usz id) const;

    // These only work if called within the same id stack
    const LayoutElement *QueryElement(const TKit::StringView label) const
    {
        return QueryElement(GetNextId(TKit::Hash(label)));
    }
    const LayoutElement *QueryElement(const CodePoint code) const
    {
        return QueryElement(GetNextId(TKit::Hash(code)));
    }

    bool IsHovered(const usz id, const f32v2 &point, const f32v2 &padding = {0.f}) const
    {
        const LayoutElement *elm = QueryElement(id);
        return elm ? elm->IsHovered(point, padding) : false;
    }

    // These only work if called within the same id stack
    bool IsHovered(const TKit::StringView label, const f32v2 &point, const f32v2 &padding = {0.f}) const
    {
        return IsHovered(GetNextId(TKit::Hash(label)), point, padding);
    }
    bool IsHovered(const CodePoint code, const f32v2 &point, const f32v2 &padding = {0.f}) const
    {
        return IsHovered(GetNextId(TKit::Hash(code)), point, padding);
    }

    void Compile();

    const TKit::TierArray<LayoutDrawInfo> &GetDrawInfo() const
    {
        return m_DrawInfo;
    }

    void PushId(const usz id)
    {
        m_IdStack.Append(GetNextId(id));
    }
    void PushId(const TKit::StringView id)
    {
        PushId(TKit::Hash(id));
    }

    template <typename T>
        requires(!std::same_as<T, char>)
    void PushId(const T *id)
    {
        PushId(TKit::Hash(id));
    }
    void PopId()
    {
        m_IdStack.Pop();
    }

  private:
    usz beginPanel(usz label, const LayoutPanelParameters &params);
    void endPanel();

    void fitPass(LayoutAxis axis);
    void growShrinkPass(LayoutAxis axis);
    void wrapText();
    void positionPass();

    TKit::TierArray<LayoutElement> m_Elements{};
    TKit::TierArray<u32> m_ElementStack{};
    TKit::TierArray<u32> m_Breadth{};
    TKit::TierArray<u32> m_ReversedBreadth{};
    TKit::TierArray<LayoutDrawInfo> m_DrawInfo{};

    TKit::TierHashMap<usz, u32> m_Map{};
    TKit::TierArray<LayoutElement> m_PreviousElements{};

    TKit::TierArray<usz> m_IdStack{};

    LayoutSpecs m_Specs{};
    usz m_AutoLabel = 0;
};
} // namespace Onyx
