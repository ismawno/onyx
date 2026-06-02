#include "pch.hpp"
#include "onyx/layout.hpp"
#include "onyx/resources.hpp"
#include "tkit/container/stack_array.hpp"

#define ONYX_LAYOUT_START_ID 14695981039346656037ULL

namespace Onyx
{
static LayoutAxis getAxis(const LayoutDirection dir)
{
    return LayoutAxis(dir >> 1);
}

Layout::Layout(const LayoutSpecs &spc) : m_Specs(spc)
{
    const DefaultResources &def = Resources::GetDefaultResources();
    const auto assign = [&](Resource &res, const Resource specific, const Resource fallback) {
        if (specific == NullHandle)
            res = fallback;
        else
            res = specific;
    };
    assign(m_Specs.Font, spc.Font, def.Font);
    assign(m_Specs.RectangleMesh, spc.RectangleMesh, def.Quad2);
    assign(m_Specs.RoundedRectangleMesh, spc.RoundedRectangleMesh, def.RoundedRect2);

    m_IdStack.Append(ONYX_LAYOUT_START_ID);
}
usz Layout::beginPanel(const usz label, const LayoutPanelParameters &params)
{
    const u32 c = m_Elements.GetSize();
    LayoutElement &current = m_Elements.Append();
    current.Id = GetNextId(label);

    if (params.Floating.Enable)
    {
        current.Type = LayoutElement_Floating;
        current.FloatAttachment = params.Floating.Attachment;
        current.FloatAlignment = params.Floating.Alignment;
        current.DrawOnTop = params.Floating.DrawOnTop;
    }
    else
        current.Type = LayoutElement_Panel;

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
        current.ClipMin = f32v2{TKIT_F32_LOWEST};
        current.ClipMax = f32v2{TKIT_F32_MAX};
    }

    current.ChildGap = params.ChildGap;
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
            if (current.Sizing[i] != LayoutSizing_Fit)
                current.Size[i] = Math::Clamp(current.Size[i], current.MinSize[i], current.MaxSize[i]);
            else
            {
                const f32 s = params.Sizing[i].ShrinkTolerance;
                TKIT_ASSERT(s >= 0.f && s <= 1.f, "[ONYX][LAYOUT] Shrink tolerance must be between 0 and 1");
                current.ShrinkTolerance[i] = s;
            }
        }

        // bc fits get clamped in the fit pass

        current.ChildOffset[i] = params.ChildOffset[i].Offset;
        current.SelfOffset[i] = params.SelfOffset[i].Offset;
        current.ChildOffsetType[i] = params.ChildOffset[i].Type;
        current.SelfOffsetType[i] = params.SelfOffset[i].Type;
    }
    current.Padding = params.Padding;
    PushId(label);
    return current.Id;
}

void Layout::endPanel()
{
    TKIT_ASSERT(!m_ElementStack.IsEmpty(),
                "[ONYX][UI] Begin()/End() Mismatch! Every Begin() must be matched with an End()");

    const u32 c = m_ElementStack.GetBack();
    if (!m_Elements[c].Children.IsEmpty())
        m_ReversedBreadth.Append(c);

    m_ElementStack.Pop();
    PopId();
}

usz Layout::Text(const usz label, const TKit::StringView text, const LayoutTextParameters &params)
{
    if (text.IsEmpty())
        return label;

    const u32 c = m_Elements.GetSize();
    LayoutElement &current = m_Elements.Append();
    current.Id = GetNextId(label);
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
    current.Material = params.Material;
    current.TextMode = params.Mode;

    const FontData &fdata = Resources::GetFontData(current.Font);

    const f32 fs = params.FontSize;
    current.FontSize = fs;
    current.Size = fs * fdata.ComputeTextSize(text);

    current.MinSize[0] = fs * fdata.ComputeTextMinimumWidth(text);
    current.MinSize[1] = 0.f;
    // current.MinSize = f32v2{0.f};
    current.MaxSize = f32v2{TKIT_F32_MAX};
    return current.Id;
}

// NOTE(Isma): A bit repetitive here with text
usz Layout::Unicode(const usz label, const CodePoint code, const LayoutUnicodeParameters &params)
{
    const u32 c = m_Elements.GetSize();

    LayoutElement &current = m_Elements.Append();
    current.Id = GetNextId(label);
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
    current.Material = params.Material;

    const FontData &fdata = Resources::GetFontData(current.Font);
    const Resource glyph = Resources::GetGlyph(current.Font, code);
    const GlyphData &gdata = Resources::GetGlyphData(glyph);

    const f32 fs = params.Size;
    current.FontSize = fs;
    current.Size = fs * f32v2{gdata.Advance, fdata.LineHeight};

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

const LayoutElement *Layout::QueryElement(const usz id) const
{
    const auto it = m_Map.Find(id);
    if (it == m_Map.end())
        return nullptr;

    const u32 idx = it->Value;
    return &m_PreviousElements[idx];
}

void Layout::fitPass(const LayoutAxis axis)
{
    for (const u32 p : m_ReversedBreadth)
    {
        LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");
        TKIT_ASSERT(parent.Type != LayoutElement_Text && parent.Type != LayoutElement_Unicode,
                    "[ONYX][LAYOUT] A parent node cannot be text or unicode");

        if (parent.Sizing[axis] != LayoutSizing_Fit)
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
        const f32 pfactor = 1.f - parent.ShrinkTolerance[axis];

        pmnsize = Math::Max(pmnsize, childMinSizeTotal) + pfactor * padding;
        psize += padding;

        if (paxis == axis)
            psize += parent.ChildGap * (parent.NonFloatChildCount - 1);

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
                const bool isGrow = child.Sizing[axis] == LayoutSizing_Grow;
                const bool isFit = child.Sizing[axis] == LayoutSizing_Fit;
                const bool isWrapped = isText && child.TextMode == TextMode_Wrapped;
                const bool canGrow = csize < child.MaxSize[axis];
                const bool canShrink = csize > child.MinSize[axis];

                if (isPanel && isGrow && canGrow)
                    toGrow.Append(c);
                if ((isWrapped || isFit) && canShrink)
                    toShrink.Append(c);
            }

            while (!toGrow.IsEmpty() && remainingSize > TKIT_F32_EPSILON)
            {
                f32 smallest = TKIT_F32_MAX;
                f32 secSmallest = TKIT_F32_MAX;
                f32 toAdd = remainingSize;
                for (const u32 c : toGrow)
                {
                    const LayoutElement &child = m_Elements[c];
                    const f32 csize = child.Size[axis];
                    if (smallest > csize)
                        smallest = csize;
                    else if (smallest < csize)
                    {
                        secSmallest = Math::Min(secSmallest, csize);
                        toAdd = secSmallest - smallest;
                    }
                }

                toAdd = Math::Min(toAdd, remainingSize / toGrow.GetSize());
                for (u32 i = toGrow.GetSize() - 1; i < toGrow.GetSize(); --i)
                {
                    const u32 c = toGrow[i];
                    LayoutElement &child = m_Elements[c];
                    f32 &csize = child.Size[axis];
                    if (!Math::Approximately(csize, smallest))
                        continue;

                    const f32 msize = child.MaxSize[axis];
                    if (csize + toAdd >= msize)
                    {
                        remainingSize -= msize - csize;
                        csize = msize;
                        toGrow.RemoveUnordered(toGrow.begin() + i);
                    }
                    else
                    {
                        csize += toAdd;
                        remainingSize -= toAdd;
                    }
                }
            }
            // NOTE(Isma): This and top part could be merged
            while (!toShrink.IsEmpty() && remainingSize < -TKIT_F32_EPSILON)
            {
                f32 biggest = 0.f;
                f32 secBiggest = 0.f;
                f32 toRemove = -remainingSize;
                for (const u32 c : toShrink)
                {
                    const LayoutElement &child = m_Elements[c];
                    const f32 csize = child.Size[axis];
                    if (biggest < csize)
                        biggest = csize;
                    else if (biggest > csize)
                    {
                        secBiggest = Math::Max(secBiggest, csize);
                        toRemove = biggest - secBiggest;
                    }
                }

                toRemove = Math::Min(toRemove, -remainingSize / toShrink.GetSize());
                for (u32 i = toShrink.GetSize() - 1; i < toShrink.GetSize(); --i)
                {
                    const u32 c = toShrink[i];
                    LayoutElement &child = m_Elements[c];
                    f32 &csize = child.Size[axis];
                    if (!Math::Approximately(csize, biggest))
                        continue;

                    const f32 msize = child.MinSize[axis];
                    if (csize - toRemove <= msize)
                    {
                        remainingSize += csize - msize;
                        csize = msize;
                        toShrink.RemoveUnordered(toShrink.begin() + i);
                    }
                    else
                    {
                        csize -= toRemove;
                        remainingSize += toRemove;
                    }
                }
            }
        }
        else
            for (const u32 c : parent.Children)
            {
                LayoutElement &child = m_Elements[c];
                if (child.Type == LayoutElement_Floating)
                    continue;

                const bool isPanel = child.Type == LayoutElement_Panel;
                const bool isText = child.Type == LayoutElement_Text;
                const bool isGrow = child.Sizing[axis] == LayoutSizing_Grow;
                const bool isFit = child.Sizing[axis] == LayoutSizing_Fit;
                const bool isHor = axis == LayoutAxis_Horizontal;
                const bool isWrapped = isText && child.TextMode == TextMode_Wrapped;

                if (isPanel && isGrow)
                    child.Size[axis] = Math::Clamp(remainingSize, child.MinSize[axis], child.MaxSize[axis]);
                else if (isFit || (isHor && isWrapped))
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
            f32 &tcsize = parent.ChildSize[axis];
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
            {
                f32 tcsize = 0.f;
                for (const u32 c : parent.Children)
                    tcsize = Math::Max(tcsize, m_Elements[c].Size[axis]);
            }

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

            const f32 clmn = parent.ClipMin[axis];
            const f32 clmx = parent.ClipMax[axis];

            const f32 pmn = ppos;
            const f32 pmx = ppos + psize;

            f32 poffset = ppos + coffset + padding + parentAlignOffset;
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

                    child.ClipMin[axis] = TKIT_F32_LOWEST;
                    child.ClipMax[axis] = TKIT_F32_MAX;
                    return;
                }

                cpos += poffset + soffset + computeChildAlignOffset(csize);
                if (parent.ChildOverflow == LayoutOverflow_Clip)
                {
                    child.ClipMin[axis] = Math::Max(pmn, clmn);
                    child.ClipMax[axis] = Math::Min(pmx, clmx);
                }
                else
                {
                    child.ClipMin[axis] = clmn;
                    child.ClipMax[axis] = clmx;
                }

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
    TKIT_ASSERT(
        m_IdStack.GetSize() == 1,
        "[ONYX][LAYOUT] Id stack size mismatch (size = {}, should be 1). For every PushId(), there must be a PopId()",
        m_IdStack.GetSize());
    fitPass(LayoutAxis_Horizontal);
    growShrinkPass(LayoutAxis_Horizontal);
    wrapText();
    fitPass(LayoutAxis_Vertical);
    growShrinkPass(LayoutAxis_Vertical);
    positionPass();

    TKit::StackArray<LayoutDrawInfo> floats{};
    floats.Reserve(m_Elements.GetSize());

    m_Map.Clear();
    m_PreviousElements.Clear();
    m_DrawInfo.Clear();

    u32 idx = 0;
    for (const LayoutElement &elm : m_Elements)
    {
        m_PreviousElements.Append(elm);
        const bool fill = !Math::ApproachesZero(elm.FillColor.rgba[3]);
        const bool outline = !Math::ApproachesZero(elm.OutlineWidth);
        const bool text = elm.Type == LayoutElement_Text;
        const bool sized = !Math::ApproachesZero(elm.Size[0]) && !Math::ApproachesZero(elm.Size[1]);

        LayoutDrawInfo info;
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
        m_Map[elm.Id] = idx++;

        if ((!fill && !outline) || (!text && !sized))
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
    m_AutoLabel = 0;
}
} // namespace Onyx
