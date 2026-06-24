#include "pch.hpp"
#include "onyx/layout.hpp"
#include "onyx/resources.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
static LayoutAxis getAxis(const LayoutDirection dir)
{
    return LayoutAxis(dir >> 1);
}

Layout::Layout(const LayoutSpecs &spc) : m_Specs(spc)
{
    applySpecDefaults();
}

#ifdef TKIT_ENABLE_ASSERTS
static void checkId(TKit::TierHashSet<usz> &elements, const LayoutId id)
{
    if (id.__DebugName.IsEmpty())
    {
        TKIT_ASSERT(!elements.Contains(id.Id),
                    "[ONYX][LAYOUT] Found conflicting ids. Attempting to introduce a layout element whose id ({}) "
                    "already exists in the layout (id name not available)",
                    id.Id);
    }
    else
    {
        TKIT_ASSERT(!elements.Contains(id.Id),
                    "[ONYX][LAYOUT] Found conflicting ids. Attempting to introduce a layout element whose id ({}) "
                    "already exists in the layout. Debug name: {}",
                    id.Id, id.__DebugName);
    }
    elements.Insert(id.Id);
}
#    define CHECK_ID(id) checkId(m_InsertedElements, id)
#else
#    define CHECK_ID(id)
#endif

usz Layout::BeginPanel(const LayoutId id, const LayoutPanelParameters &params)
{
    TKIT_ASSERT(params.Texture == NullHandle || params.Material == NullHandle,
                "[ONYX][LAYOUT] Cannot specify both material and texture at the same time");

    CHECK_ID(id);

    const u32 c = m_Elements.GetSize();
    LayoutElement &current = m_Elements.Append();
    current.Id = id.Id;

    if (params.Floating.Enable)
    {
        current.Type = LayoutElement_Floating;
        current.FloatAttachment = params.Floating.Attachment;
        current.FloatAlignment = params.Floating.Alignment;
        current.FloatClip = params.Floating.Clip;
        current.DrawOnTop = params.Floating.DrawOnTop;
    }
    else
    {
        current.Type = LayoutElement_Panel;
        current.DrawOnTop = false;
    }

    const u32 p = m_ElementStack.IsEmpty() ? TKIT_U32_MAX : m_ElementStack.GetBack();
    current.Parent = p;
    m_ElementStack.Append(c);
    if (p != TKIT_U32_MAX)
    {
        LayoutElement &parent = m_Elements[current.Parent];
        if (parent.Children.IsEmpty())
            m_Breadth.Append(current.Parent);
        parent.Children.Append(c);

        if (!params.Floating.Enable)
            ++parent.NonFloatChildCount;

        current.SelfOverflow = parent.ChildOverflow;
        current.DrawOnTop |= parent.DrawOnTop;
    }
    else
    {
        TKIT_ASSERT(params.Sizing[0].Type != LayoutSizing_Normalized &&
                        params.Sizing[1].Type != LayoutSizing_Normalized,
                    "[ONYX][LAYOUT] The root layout element cannot have normalized sizing");
        TKIT_ASSERT(params.SelfOffset[0].Type != LayoutOffset_Normalized &&
                        params.SelfOffset[1].Type != LayoutOffset_Normalized,
                    "[ONYX][LAYOUT] The root layout element cannot have normalized offsets");

        TKIT_ASSERT(!params.Floating.Enable, "[ONYX][LAYOUT] The root layout element cannot be floating");
        current.ClipMin = f32v2{TKIT_F32_MIN};
        current.ClipMax = f32v2{TKIT_F32_MAX};
    }

    current.ChildGap = params.ChildGap;
    current.Texture = params.Texture;
    current.TexOffset = params.TexOffset;
    current.TexScale = params.TexScale;
    current.Material = params.Material;
    current.FillColor = params.FillColor;
    current.OutlineColor = params.OutlineColor;
    current.OutlineWidth = params.OutlineWidth;
    current.Direction = params.Direction;
    current.Alignment = params.Alignment;
    current.ChildOverflow = params.Overflow;
    current.Shape = params.Shape;

    if (current.Shape.Type != LayoutShape_Circle && current.Shape.Handle == NullHandle)
    {
        Resource handle = NullHandle;
        switch (params.Shape.Type)
        {
        case LayoutShape_Rectangle:
            handle = m_Specs.RectangleMesh;
            break;
        case LayoutShape_RoundedRectangle:
            handle = m_Specs.RoundedRectangleMesh;
            break;
        default:
            break;
        }
        current.Shape.Handle = handle;
    }

    for (u32 i = 0; i < 2; ++i)
    {
        // NOTE(Isma): This could be removed and hope the user sets the sizing correctly with the static methods
        current.Sizing[i] = params.Sizing[i].Type;
        if (params.Sizing[i].Type == LayoutSizing_Absolute || params.Sizing[i].Type == LayoutSizing_Normalized)
        {
            current.Size[i] = params.Sizing[i].Size;
            current.MinSize[i] = current.Size[i];
            current.MaxSize[i] = current.Size[i];
        }
        else
        {
            current.Size[i] = 0.f;
            current.MinSize[i] = params.Sizing[i].Min;
            current.MaxSize[i] = params.Sizing[i].Max;
            if (current.Sizing[i] != LayoutSizing_Fit && current.Sizing[i] != LayoutSizing_Flex)
                current.Size[i] = Math::Clamp(current.Size[i], current.MinSize[i], current.MaxSize[i]);
        }

        // bc fits get clamped in the fit pass

        current.ChildOffset[i] = params.ChildOffset[i].Offset;
        current.SelfOffset[i] = params.SelfOffset[i].Offset;
        current.ChildOffsetType[i] = params.ChildOffset[i].Type;
        current.SelfOffsetType[i] = params.SelfOffset[i].Type;
    }
    current.Padding = params.Padding;
    return current.Id;
}

void Layout::EndPanel()
{
    TKIT_ASSERT(!m_ElementStack.IsEmpty(),
                "[ONYX][LAYOUT] Begin()/End() Mismatch! Every Begin() must be matched with an End()");

    const u32 c = m_ElementStack.GetBack();
    if (m_Elements[c].NonFloatChildCount != 0)
        m_ReversedBreadth.Append(c);

    m_ElementStack.Pop();
}

usz Layout::Text(const LayoutId id, const TKit::StringView text, const LayoutTextParameters &params)
{
    TKIT_ASSERT(params.Texture == NullHandle || params.Material == NullHandle,
                "[ONYX][LAYOUT] Cannot specify both material and texture at the same time");
    CHECK_ID(id);
    const u32 c = m_Elements.GetSize();
    LayoutElement &current = m_Elements.Append();
    current.Id = id.Id;
    current.Type = LayoutElement_Text;
    current.Shape.Type = LayoutShape_Text;

    const u32 p = m_ElementStack.IsEmpty() ? TKIT_U32_MAX : m_ElementStack.GetBack();
    TKIT_ASSERT(p != TKIT_U32_MAX, "[ONYX][LAYOUT] A text element cannot be a root ui element");
    current.Parent = p;

    LayoutElement &parent = m_Elements[current.Parent];
    if (parent.Children.IsEmpty())
        m_Breadth.Append(current.Parent);

    parent.Children.Append(c);
    ++parent.NonFloatChildCount;

    current.Alignment = parent.Alignment;
    current.SelfOverflow = parent.ChildOverflow;
    current.DrawOnTop = parent.DrawOnTop;

    for (u32 i = 0; i < 2; ++i)
    {
        current.SelfOffset[i] = params.Offset[i].Offset;
        current.SelfOffsetType[i] = params.Offset[i].Type;
    }
    current.FillColor = params.FillColor;
    current.OutlineColor = params.OutlineColor;
    current.OutlineWidth = params.OutlineWidth;
    current.Text = TKit::String{text.GetData(), text.GetSize()};
    // NOTE(Isma): This is a weak check. If user passes a bad font that is not NullHandle, it will go through
    current.Font = params.Font == NullHandle ? m_Specs.Font : params.Font;
    current.Texture = params.Texture;
    current.TexOffset = params.TexOffset;
    current.TexScale = params.TexScale;
    current.Material = params.Material;
    current.TextMode = params.Mode;

    const FontData &fdata = Resources::GetFontData(current.Font);

    const f32 fs = params.FontSize;
    current.FontSize = fs;
    current.Size = Math::Max(fs * fdata.ComputeTextSize(text), params.MinSize);

    current.MinSize[0] =
        current.TextMode == TextMode_Wrapped ? (fs * fdata.ComputeTextMinimumWidth(text)) : current.Size[0];
    current.MinSize[1] = 0.f; // this is set in wrapText. no problem that this is zero

    current.MinSize = Math::Max(current.MinSize, params.MinSize);
    current.MaxSize = f32v2{TKIT_F32_MAX};
    return current.Id;
}

// NOTE(Isma): A bit repetitive here with text
usz Layout::Unicode(const LayoutId id, const CodePoint code, const LayoutUnicodeParameters &params)
{
    TKIT_ASSERT(params.Texture == NullHandle || params.Material == NullHandle,
                "[ONYX][LAYOUT] Cannot specify both material and texture at the same time");
    CHECK_ID(id);

    const u32 c = m_Elements.GetSize();

    LayoutElement &current = m_Elements.Append();
    current.Id = id.Id;
    current.Type = LayoutElement_Unicode;
    current.Shape.Type = LayoutShape_Unicode;

    const u32 p = m_ElementStack.IsEmpty() ? TKIT_U32_MAX : m_ElementStack.GetBack();
    TKIT_ASSERT(p != TKIT_U32_MAX, "[ONYX][LAYOUT] A unicode element cannot be a root ui element");
    current.Parent = p;

    LayoutElement &parent = m_Elements[current.Parent];
    if (parent.Children.IsEmpty())
        m_Breadth.Append(current.Parent);

    parent.Children.Append(c);
    ++parent.NonFloatChildCount;

    current.Alignment = parent.Alignment;
    current.SelfOverflow = parent.ChildOverflow;
    current.DrawOnTop = parent.DrawOnTop;
    for (u32 i = 0; i < 2; ++i)
    {
        current.SelfOffset[i] = params.Offset[i].Offset;
        current.SelfOffsetType[i] = params.Offset[i].Type;
    }
    current.FillColor = params.FillColor;
    current.OutlineColor = params.OutlineColor;
    current.OutlineWidth = params.OutlineWidth;
    current.Unicode = code;
    current.Font = params.Font == NullHandle ? m_Specs.Font : params.Font;
    current.Texture = params.Texture;
    current.TexOffset = params.TexOffset;
    current.TexScale = params.TexScale;
    current.Material = params.Material;

    const FontData &fdata = Resources::GetFontData(current.Font);
    const Resource glyph = Resources::GetGlyph(current.Font, code);
    const GlyphData &gdata = Resources::GetGlyphData(glyph);

    const f32 fs = params.Size;
    current.FontSize = fs;
    current.Size = Math::Max(fs * f32v2{gdata.Advance, fdata.LineHeight}, params.MinSize);

    current.MinSize = current.Size;
    current.MaxSize = f32v2{TKIT_F32_MAX};
    return current.Id;
}

bool LayoutElement::IsHovered(const f32v2 &pos, const f32v2 &padding) const
{
    const f32v2 hpad = 0.5f * padding;

    const f32v2 mn = Position - hpad;
    const f32v2 mx = Position + Size + hpad;

    const f32v2 cmn = ClipMin - hpad;
    const f32v2 cmx = ClipMax + hpad;

    const auto check = [](const f32v2 &p, const f32v2 &mn, const f32v2 &mx) {
        return p[0] >= mn[0] && p[0] <= mx[0] && p[1] >= mn[1] && p[1] <= mx[1];
    };

    return check(pos, mn, mx) && check(pos, cmn, cmx);
}

const LayoutElement *Layout::QueryElement(const LayoutId id) const
{
    const auto it = m_ElementMap.Find(id.Id);
    if (it == m_ElementMap.end())
        return nullptr;

    return &it->Value;
}

void Layout::fitPass(const LayoutAxis axis)
{
    for (const u32 p : m_ReversedBreadth)
    {
        LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(parent.Type != LayoutElement_Text && parent.Type != LayoutElement_Unicode,
                    "[ONYX][LAYOUT] A parent node cannot be text or unicode");

        if (parent.Sizing[axis] != LayoutSizing_Fit && parent.Sizing[axis] != LayoutSizing_Flex)
            continue;

        const LayoutAxis paxis = getAxis(parent.Direction);

        f32 &psize = parent.Size[axis];
        f32 &pmnsize = parent.MinSize[axis];

        f32 childMinSizeTotal = 0.f;
        const f32 pmxsize = parent.MaxSize[axis];
        for (const u32 c : parent.Children)
        {
            const LayoutElement &child = m_Elements[c];
            if (child.Sizing[axis] == LayoutSizing_Normalized || child.Type == LayoutElement_Floating)
                continue;

            const f32 csize = child.Size[axis];
            const f32 cmnsize = child.MinSize[axis];

            TKIT_ASSERT(child.Parent != TKIT_U32_MAX, "[ONYX][LAYOUT] Only the root node can have no parent");
            if (paxis == axis)
            {
                psize += csize;
                childMinSizeTotal += cmnsize;
            }
            else
            {
                psize = Math::Max(psize, csize);
                childMinSizeTotal = Math::Max(childMinSizeTotal, cmnsize);
            }
        }
        const f32 padding = parent.Padding[2 * axis] + parent.Padding[2 * axis + 1];
        const f32 cgap = paxis == axis ? (parent.ChildGap * (parent.NonFloatChildCount - 1)) : 0.f;

        pmnsize = Math::Max(pmnsize, childMinSizeTotal + padding + cgap);
        pmnsize = Math::Min(pmnsize, pmxsize);
        psize += padding + cgap;

        psize = Math::Clamp(psize, pmnsize, pmxsize);
    }
}

void Layout::growShrinkPass(const LayoutAxis axis)
{
    for (const u32 p : m_Breadth)
    {
        const LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");

        const f32 padding = parent.Padding[2 * axis] + parent.Padding[2 * axis + 1];
        const f32 floatFactor = parent.Size[axis];
        const f32 normFactor = Math::Max(0.f, floatFactor - padding);
        for (const u32 c : parent.Children)
        {
            LayoutElement &child = m_Elements[c];
            if (child.Sizing[axis] == LayoutSizing_Normalized)
            {
                const f32 factor = child.Type == LayoutElement_Floating ? floatFactor : normFactor;
                child.Size[axis] *= factor;
                child.MinSize[axis] *= factor;
                child.MaxSize[axis] *= factor;
            }
        }

        const LayoutAxis paxis = getAxis(parent.Direction);
        f32 remainingSize = parent.Size[axis] - padding;
        if (paxis == axis)
        {
            remainingSize -= parent.ChildGap * (parent.NonFloatChildCount - 1);
            TKit::StackArray<u32> toGrow{};
            toGrow.Reserve(m_Elements.GetSize());
            TKit::StackArray<u32> toShrink{};
            toShrink.Reserve(m_Elements.GetSize());

            for (const u32 c : parent.Children)
            {
                const LayoutElement &child = m_Elements[c];
                if (child.Type == LayoutElement_Floating)
                    continue;

                const f32 csize = child.Size[axis];
                remainingSize -= csize;

                const bool isPanel = child.Type == LayoutElement_Panel;
                const bool isText = child.Type == LayoutElement_Text;
                const bool isFlex = child.Sizing[axis] == LayoutSizing_Flex;
                const bool isGrow = isFlex || child.Sizing[axis] == LayoutSizing_Grow;
                const bool isFit = isFlex || child.Sizing[axis] == LayoutSizing_Fit;
                const bool isWrapped = isText && child.TextMode == TextMode_Wrapped;
                const bool canGrow = csize < child.MaxSize[axis];
                const bool canShrink = csize > child.MinSize[axis];

                if (isPanel && isGrow && canGrow)
                    toGrow.Append(c);
                if ((isWrapped || isFit) && canShrink)
                    toShrink.Append(c);
            }

            const auto distribute = [&](TKit::StackArray<u32> &candidates, const f32 sign) {
                f32 extremal = sign > 0.f ? TKIT_F32_MAX : 0.f;
                f32 secExtremal = extremal;
                f32 budget = sign * remainingSize;

                for (const u32 c : candidates)
                {
                    const LayoutElement &child = m_Elements[c];
                    const f32 csize = sign * child.Size[axis];
                    if (extremal > csize)
                        extremal = csize;
                    else if (extremal < csize)
                    {
                        secExtremal = Math::Min(secExtremal, csize);
                        budget = secExtremal - extremal;
                    }
                }

                const u32 ccount = candidates.GetSize();
                budget = Math::Min(budget, sign * remainingSize / ccount);

                for (u32 i = candidates.GetSize() - 1; i < candidates.GetSize(); --i)
                {
                    const u32 c = candidates[i];
                    LayoutElement &child = m_Elements[c];
                    f32 &csize = child.Size[axis];
                    if (!Math::Approximately(csize, sign * extremal))
                        continue;

                    const bool growing = sign > 0.f;
                    const f32 msize = growing ? child.MaxSize[axis] : child.MinSize[axis];
                    if (sign * csize + budget >= sign * msize)
                    {
                        remainingSize += sign * (csize - msize);
                        csize = msize;
                        candidates.RemoveUnordered(candidates.begin() + i);
                    }
                    else
                    {
                        csize += sign * budget;
                        remainingSize -= sign * budget;
                    }
                }
            };

            while (!toGrow.IsEmpty() && remainingSize > TKIT_F32_EPSILON)
                distribute(toGrow, 1.f);
            while (!toShrink.IsEmpty() && remainingSize < -TKIT_F32_EPSILON)
                distribute(toShrink, -1.f);
        }
        else
            for (const u32 c : parent.Children)
            {
                LayoutElement &child = m_Elements[c];
                if (child.Type == LayoutElement_Floating)
                    continue;

                const bool isPanel = child.Type == LayoutElement_Panel;
                const bool isText = child.Type == LayoutElement_Text;
                const bool isFlex = child.Sizing[axis] == LayoutSizing_Flex;
                const bool isGrow = isFlex || child.Sizing[axis] == LayoutSizing_Grow;
                const bool isFit = isFlex || child.Sizing[axis] == LayoutSizing_Fit;
                const bool isHor = axis == LayoutAxis_Horizontal;
                const bool isWrapped = isText && child.TextMode == TextMode_Wrapped;

                // size is now the remaining size of the parent. straightforward for non layout direction
                if (isPanel && isGrow)
                    child.Size[axis] = Math::Clamp(remainingSize, child.MinSize[axis], child.MaxSize[axis]);
                // check if current size is overflowing. if it is, bring it back to remainingSize, unless its too small
                // in minSize terms
                if ((isPanel && isFit) || (isHor && isWrapped))
                    child.Size[axis] = Math::Min(child.Size[axis], Math::Max(remainingSize, child.MinSize[axis]));
            }
    }
}

void Layout::wrapText()
{
    for (LayoutElement &elm : m_Elements)
    {
        if (elm.Type != LayoutElement_Text && elm.Type != LayoutElement_Unicode)
            continue;

        const FontData &fdata = Resources::GetFontData(elm.Font);
        const f32 fs = elm.FontSize;
        if (elm.TextMode == TextMode_Wrapped)
            elm.Text = fdata.WrapText(elm.Text, (elm.Size[0] + 0.01f) / fs);

        elm.Size[1] = fs * fdata.ComputeTextHeight(elm.Text);
        elm.MinSize[1] = elm.Size[1];
    }
}

void Layout::positionPass()
{
    if (m_Elements.IsEmpty())
        return;

    LayoutElement &root = m_Elements.GetFront();
    for (u32 axis = 0; axis < 2; ++axis)
        if (m_Specs.RootAlignment[axis] == Alignment_Mirrored)
            root.Position[axis] -= root.Size[axis];
        else if (m_Specs.RootAlignment[axis] == Alignment_Center)
            root.Position[axis] -= 0.5f * root.Size[axis];

    root.Position += root.SelfOffset;
    for (const u32 p : m_Breadth)
    {
        LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");

        const LayoutDirection dir = parent.Direction;
        const LayoutAxis paxis = getAxis(dir);
        const f32 cgap = parent.ChildGap;

        for (u32 axis = 0; axis < 2; ++axis)
        {
            f32 &tcsize = parent.ChildrenSize[axis];
            tcsize = 0.f;
            if (paxis == axis)
            {
                for (const u32 c : parent.Children)
                {
                    const LayoutElement &child = m_Elements[c];
                    if (child.Type != LayoutElement_Floating)
                        tcsize += m_Elements[c].Size[axis];
                }
                tcsize += cgap * (parent.NonFloatChildCount - 1);
            }
            else
                for (const u32 c : parent.Children)
                    tcsize = Math::Max(tcsize, m_Elements[c].Size[axis]);

            const f32 p0 = parent.Padding[2 * axis];
            const f32 p1 = parent.Padding[2 * axis + 1];

            const Alignment alg = parent.Alignment[axis];

            const auto computePadding = [&] {
                if (alg == Alignment_Canonical)
                    return p0;
                if (alg == Alignment_Mirrored)
                    return -p1;
                return p0 - p1;
            };

            const f32 padding = computePadding();
            const f32 psize = parent.Size[axis];
            const f32 ppos = parent.Position[axis];

            const auto computeAlignOffset = [&](const f32 size) {
                if (alg == Alignment_Mirrored)
                    return psize - size;
                if (alg == Alignment_Center)
                    return 0.5f * (psize - size);
                return 0.f;
            };

            const f32 parentAlignOffset = paxis == axis ? computeAlignOffset(tcsize) : 0.f;
            const auto computeChildAlignOffset = [&](const f32 size) {
                return paxis != axis ? computeAlignOffset(size) : 0.f;
            };

            const f32 offsetNormFactor = psize - p0 - p1;

            f32 coffset = parent.ChildOffset[axis];
            if (parent.ChildOffsetType[axis] != LayoutOffset_Absolute)
            {
                const f32 factor = parent.ChildOffsetType[axis] == LayoutOffset_Normalized
                                       ? offsetNormFactor
                                       : (offsetNormFactor - tcsize);
                coffset *= Math::Absolute(factor);
            }

            const f32 mn = parent.ClipMin[axis];
            const f32 mx = parent.ClipMax[axis];

            const f32 pmx = ppos + psize - p1;
            const f32 pmn = ppos + p0;

            f32 poffset = ppos + coffset + padding + parentAlignOffset;

            const auto clipChild = [&](LayoutElement &child) {
                f32 &cmn = child.ClipMin[axis];
                f32 &cmx = child.ClipMax[axis];
                if (parent.ChildOverflow == LayoutOverflow_Clip)
                {
                    cmn = Math::Max(pmn, mn);
                    cmx = Math::Min(pmx, mx);
                    if (cmn > cmx)
                        cmn = cmx;
                }
                else
                {
                    cmn = mn;
                    cmx = mx;
                }
            };

            const auto processChild = [&](const u32 c) {
                LayoutElement &child = m_Elements[c];

                const f32 csize = child.Size[axis];
                f32 &cpos = child.Position[axis];

                f32 soffset = child.SelfOffset[axis];
                if (child.SelfOffsetType[axis] != LayoutOffset_Absolute)
                {
                    const f32 factor = child.SelfOffsetType[axis] == LayoutOffset_Normalized
                                           ? offsetNormFactor
                                           : (offsetNormFactor - csize);
                    soffset *= Math::Absolute(factor);
                }

                if (child.Type == LayoutElement_Floating)
                {
                    cpos = ppos + coffset + soffset;
                    const LayoutAttachment fatt = child.FloatAttachment[axis];
                    const Alignment falg = child.FloatAlignment[axis];
                    if (falg == Alignment_Mirrored)
                        cpos -= csize;
                    else if (falg == Alignment_Center)
                        cpos -= 0.5f * csize;

                    if (fatt == LayoutAttachment_Mirrored)
                        cpos += psize;
                    else if (fatt == LayoutAttachment_Center)
                        cpos += 0.5f * psize;

                    if (child.FloatClip)
                        clipChild(child);
                    else
                    {
                        child.ClipMin[axis] = TKIT_F32_MIN;
                        child.ClipMax[axis] = TKIT_F32_MAX;
                    }
                    return;
                }

                cpos += poffset + soffset + computeChildAlignOffset(csize);
                clipChild(child);

                if (paxis == axis)
                    poffset += csize + cgap;
            };

            const bool naturalDir = dir == LayoutDirection_LeftToRight || dir == LayoutDirection_BottomToTop;
            if (naturalDir)
                for (const u32 c : parent.Children)
                    processChild(c);
            else
                for (u32 i = parent.Children.GetSize() - 1; i < parent.Children.GetSize(); --i)
                    processChild(parent.Children[i]);
        }
    }
}

void Layout::Compile()
{
    TKIT_ASSERT(m_ElementStack.IsEmpty(), "[ONYX][LAYOUT] Trying to compile a layout that has {} open nodes!",
                m_ElementStack.GetSize());
    fitPass(LayoutAxis_Horizontal);
    growShrinkPass(LayoutAxis_Horizontal);
    wrapText();
    fitPass(LayoutAxis_Vertical);
    growShrinkPass(LayoutAxis_Vertical);
    positionPass();

    TKit::StackArray<LayoutDrawInfo> floats{};
    floats.Reserve(m_Elements.GetSize());

    m_DrawInfo.Clear();

    for (const LayoutElement &elm : m_Elements)
    {
        m_ElementMap[elm.Id] = elm;
        const bool fill = !Math::ApproachesZero(elm.FillColor.rgba[3]);
        const bool outline = !Math::ApproachesZero(elm.OutlineWidth);
        const bool sized = !Math::ApproachesZero(elm.Size[0]) && !Math::ApproachesZero(elm.Size[1]);

        LayoutDrawInfo info;
        info.Texture = elm.Texture;
        info.TexOffset = elm.TexOffset;
        info.TexScale = elm.TexScale;
        info.Material = elm.Material;
        info.RenderFlags = 0;
        if (fill)
        {
            // NOTE(Isma): This is a weak check. If user passes a bad material that is not NullHandle, it will go
            // through
            info.RenderFlags |= elm.Material == NullHandle ? RenderModeFlag_Flat : RenderModeFlag_Shaded;
            info.FillColor = elm.FillColor;
        }
        if (outline)
        {
            info.RenderFlags |= RenderModeFlag_Outlined;
            info.OutlineColor = elm.OutlineColor;
            info.OutlineWidth = elm.OutlineWidth;
        }

        info.Position = elm.Position;
        info.ClipMin = elm.ClipMin;
        info.ClipMax = elm.ClipMax;
        info.ShapeType = elm.Shape.Type;
        switch (elm.Shape.Type)
        {
        case LayoutShape_Circle:
        case LayoutShape_Rectangle:
        case LayoutShape_Glyph:
        case LayoutShape_Custom:
            info.Handle = elm.Shape.Handle;
            info.Size = elm.Size;
            break;
        case LayoutShape_RoundedRectangle:
            info.Handle = elm.Shape.Handle;
            info.Radius = Math::Min(elm.Shape.Radius, 0.5f * Math::Min(elm.Size[0], elm.Size[1]));
            info.Size = elm.Size - 2.f * info.Radius;
            break;
        case LayoutShape_Text:
            info.Handle = elm.Font;
            info.Text = elm.Text;
            info.Size = f32v2{elm.FontSize};
            break;
        case LayoutShape_Unicode:
            info.Handle = elm.Font;
            info.Unicode = elm.Unicode;
            info.Size = f32v2{elm.FontSize};
            break;
        }

        if ((!fill && !outline) || !sized)
            continue;

        if (elm.DrawOnTop)
            floats.Append(info);
        else
            m_DrawInfo.Append(info);
    }
    m_DrawInfo.Insert(m_DrawInfo.end(), floats.begin(), floats.end());

    m_Elements.Clear();
    m_Breadth.Clear();
    m_ReversedBreadth.Clear();
    m_AutoId = 0;
#ifdef TKIT_ENABLE_ASSERTS
    m_InsertedElements.Clear();
#endif
}
void Layout::applySpecDefaults()
{
    const DefaultResources &def = Resources::GetDefaultResources();
    const auto assign = [&](Resource &res, const Resource fallback) {
        if (res == NullHandle)
            res = fallback;
    };
    assign(m_Specs.Font, def.Font);
    assign(m_Specs.RectangleMesh, def.Quad2);
    assign(m_Specs.RoundedRectangleMesh, def.RoundedRect2);
}
} // namespace Onyx
