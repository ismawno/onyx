#include "pch.hpp"
#include "onyx/layout.hpp"
#include "onyx/resources.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
Layout::Layout(const LayoutSpecs &spc)
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
}
void Layout::BeginPanel(const LayoutPanelParameters &params)
{
    const u32 c = m_Elements.GetSize();
    LayoutElement &current = m_Elements.Append();
    if (params.Floating.Enable)
    {
        current.Type = LayoutElement_Floating;
        current.FloatAttachment = params.Floating.Attachment;
        current.FloatAlignment = params.Floating.Alignment;
        current.FloatSibling = true;
    }
    else
        current.Type = LayoutElement_Panel;

    const u32 p = m_Stack.IsEmpty() ? TKIT_U32_MAX : m_Stack.GetBack();
    current.Parent = p;
    m_Stack.Append(c);
    if (p != TKIT_U32_MAX)
    {
        LayoutElement &parent = m_Elements[current.Parent];
        if (parent.Children.IsEmpty())
            m_Breadth.Append(current.Parent);
        parent.Children.Append(c);
        current.SelfOverflow = parent.ChildOverflow;
        current.FloatSibling |= parent.FloatSibling;
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
        }

        // bc fits get clamped in the fit pass

        current.ChildOffset[i] = params.ChildOffset[i].Offset;
        current.SelfOffset[i] = params.SelfOffset[i].Offset;
        current.ChildOffsetType[i] = params.ChildOffset[i].Type;
        current.SelfOffsetType[i] = params.SelfOffset[i].Type;
    }
    current.Padding = params.Padding;
}

void Layout::EndPanel()
{
    TKIT_ASSERT(!m_Stack.IsEmpty(), "[ONYX][UI] Begin()/End() Mismatch! Every Begin() must be matched with an End()");

    const u32 c = m_Stack.GetBack();
    if (!m_Elements[c].Children.IsEmpty())
        m_ReversedBreadth.Append(c);

    m_Stack.Pop();
}

void Layout::Text(const TKit::StringView text, const LayoutTextParameters &params)
{
    if (text.IsEmpty())
        return;
    const u32 c = m_Elements.GetSize();
    LayoutElement &current = m_Elements.Append();
    current.Type = LayoutElement_Text;

    const u32 p = m_Stack.IsEmpty() ? TKIT_U32_MAX : m_Stack.GetBack();
    TKIT_ASSERT(p != TKIT_U32_MAX, "[ONYX][LAYOUT] A text element cannot be a root ui element");
    current.Parent = p;

    LayoutElement &parent = m_Elements[current.Parent];
    if (parent.Children.IsEmpty())
        m_Breadth.Append(current.Parent);
    parent.Children.Append(c);
    current.Alignment = parent.Alignment;
    current.SelfOverflow = parent.ChildOverflow;
    current.FloatSibling = parent.FloatSibling;

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

    current.MinSize[0] = 0.f; // fs * fdata.ComputeTextMinimumWidth(text);
    current.MinSize[1] = 0.f;

    current.MaxSize = f32v2{TKIT_F32_MAX};
}

void Layout::fitPass(const LayoutAxis axis)
{
    for (const u32 p : m_ReversedBreadth)
    {
        LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");
        if (parent.Type == LayoutElement_Text || parent.Sizing[axis] != LayoutSizing_Fit)
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
        pmnsize = Math::Max(pmnsize, childMinSizeTotal);

        const f32 padding = parent.Padding[2 * axis] + parent.Padding[2 * axis + 1];
        psize += padding;

        if (paxis == axis)
            psize += parent.ChildGap * (parent.Children.GetSize() - 1);

        psize = Math::Clamp(psize, pmnsize, pmxsize);
    }
}

void Layout::normPass(const LayoutAxis axis)
{
    for (const u32 p : m_Breadth)
    {
        const LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");

        const f32 padding = parent.Padding[2 * axis] + parent.Padding[2 * axis + 1];
        const f32 factor = Math::Max(0.f, parent.Size[axis] - padding);
        for (const u32 c : parent.Children)
        {
            LayoutElement &child = m_Elements[c];
            if (child.Sizing[axis] == LayoutSizing_Normalized)
            {
                child.Size[axis] *= factor;
                child.MinSize[axis] *= factor;
                child.MaxSize[axis] *= factor;
            }
        }
    }
}

void Layout::growShrinkPass(const LayoutAxis axis)
{
    for (const u32 p : m_Breadth)
    {
        const LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");

        const LayoutAxis paxis = getAxis(parent.Direction);
        const f32 padding = parent.Padding[2 * axis] + parent.Padding[2 * axis + 1];
        f32 remainingSize = parent.Size[axis] - padding;
        if (paxis == axis)
        {
            remainingSize -= parent.ChildGap * (parent.Children.GetSize() - 1);
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
                    if (!TKit::Approximately(csize, smallest))
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
                    if (!TKit::Approximately(csize, biggest))
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
        if (elm.Type == LayoutElement_Text && elm.TextMode == TextMode_Wrapped)
        {
            const FontData &fdata = Resources::GetFontData(elm.Font);
            const f32 fs = elm.FontSize;
            elm.Text = fdata.WrapText(elm.Text, (elm.Size[0] + 0.01f) / fs);
            elm.Size[1] = fs * fdata.ComputeTextHeight(elm.Text);
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

    for (const u32 p : m_Breadth)
    {
        const LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");

        const LayoutDirection dir = parent.Direction;
        const LayoutAxis paxis = getAxis(dir);
        const f32 cgap = parent.ChildGap;

        for (u32 axis = 0; axis < 2; ++axis)
        {
            f32 tcs;
            bool tcomputed = false;
            const auto computeTotalChildrenSize = [&] {
                if (tcomputed)
                    return tcs;
                tcomputed = true;

                tcs = 0.f;
                for (const u32 c : parent.Children)
                    tcs += m_Elements[c].Size[axis];
                tcs += cgap * (parent.Children.GetSize() - 1);
                return tcs;
            };

            const auto computeMaxChildrenSize = [&] {
                f32 mcs = 0.f;
                for (const u32 c : parent.Children)
                    mcs = Math::Max(mcs, m_Elements[c].Size[axis]);
                return mcs;
            };

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

            const f32 psize = parent.Size[axis];
            const f32 ppos = parent.Position[axis];

            const auto computeAlignOffset = [&](const bool aligned, const f32 size) {
                if (!aligned)
                    return 0.f;
                if (alg == Alignment_Mirrored)
                    return psize - size;
                if (alg == Alignment_Center)
                    return 0.5f * (psize - size);
                return 0.f;
            };
            const auto computeParentAlignOffset = [&] {
                return computeAlignOffset(paxis == axis, computeTotalChildrenSize());
            };
            const auto computeChildAlignOffset = [&](const f32 size) {
                return computeAlignOffset(paxis != axis, size);
            };

            const f32 offsetNormFactor = psize - p0 - p1;

            f32 coffset = parent.ChildOffset[axis];
            if (parent.ChildOffsetType[axis] != LayoutOffset_Absolute)
            {
                const f32 factor =
                    parent.ChildOffsetType[axis] == LayoutOffset_Normalized
                        ? offsetNormFactor
                        : (offsetNormFactor - (paxis == axis ? computeTotalChildrenSize() : computeMaxChildrenSize()));
                coffset *= Math::Absolute(factor);
            }

            const f32 clmn = parent.ClipMin[axis];
            const f32 clmx = parent.ClipMax[axis];

            const f32 pmn = ppos;
            const f32 pmx = ppos + psize;

            f32 poffset = ppos + coffset + computePadding() + computeParentAlignOffset();
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
    TKIT_ASSERT(m_Stack.IsEmpty(), "[ONYX][LAYOUT] Trying to compile a layout that has {} open nodes!",
                m_Stack.GetSize());
    m_ElementInfo.Clear();
    fitPass(LayoutAxis_Horizontal);
    normPass(LayoutAxis_Horizontal);
    growShrinkPass(LayoutAxis_Horizontal);
    wrapText();
    fitPass(LayoutAxis_Vertical);
    normPass(LayoutAxis_Vertical);
    growShrinkPass(LayoutAxis_Vertical);
    positionPass();

    TKit::StackArray<LayoutElementInfo> floats{};
    floats.Reserve(m_Elements.GetSize());
    for (const LayoutElement &elm : m_Elements)
    {
        const bool fill = !TKit::ApproachesZero(elm.FillColor.rgba[3]);
        const bool outline = !TKit::ApproachesZero(elm.OutlineWidth);
        const bool text = elm.Type == LayoutElement_Text;
        const bool sized = !TKit::ApproachesZero(elm.Size[0]) && !TKit::ApproachesZero(elm.Size[1]);

        if ((!fill && !outline) || (!text && !sized))
            continue;

        LayoutElementInfo &info = elm.FloatSibling ? floats.Append() : m_ElementInfo.Append();
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
        if (text)
        {
            info.Geo = Geometry_Glyph;
            info.Handle = elm.Font;
            info.Text = elm.Text;
            info.Size = f32v2{elm.FontSize};
        }
        else
        {
            info.ShapeType = elm.Shape.Type;
            info.Handle = elm.Shape.Handle;
            switch (elm.Shape.Type)
            {
            case LayoutShape_Circle:
                info.Geo = Geometry_Circle;
                info.Size = elm.Size;
                break;
            case LayoutShape_Custom:
            case LayoutShape_Rectangle:
                info.Geo = Geometry_Static;
                info.Size = elm.Size;
                break;
            // case LayoutShape_Stadium: {
            //     info.Geo = Geometry_Parametric;
            //     const f32 smallAxis = Math::Min(elm.Size[0], elm.Size[1]);
            //     info.Size[0] = 0.5f * smallAxis;
            //     info.Size[1] = elm.Size[1] - smallAxis;
            //     break;
            // }
            case LayoutShape_RoundedRectangle:
                info.Geo = Geometry_Parametric;
                info.Radius = Math::Min(elm.Shape.Radius, 0.5f * Math::Min(elm.Size[0], elm.Size[1]));
                info.Size = elm.Size - 2.f * info.Radius;
                break;
            }
        }
    }
    m_ElementInfo.Insert(m_ElementInfo.end(), floats.begin(), floats.end());

    m_Elements.Clear();
    m_Breadth.Clear();
    m_ReversedBreadth.Clear();
}
} // namespace Onyx
