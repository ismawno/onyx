#include "pch.hpp"
#include "onyx/ui.hpp"
#include "onyx/resources.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
void Layout::BeginPanel(const LayoutPanelParameters &params)
{
    const u32 c = m_Elements.GetSize();
    LayoutElement &current = m_Elements.Append();
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
        current.SelfAlignment = parent.ChildAlignment;
        current.SelfOverflow = parent.ChildOverflow;
    }
    else
    {
        current.SelfAlignment = params.Alignment;
        current.ClipMin = f32v2{TKIT_F32_LOWEST};
        current.ClipMax = f32v2{TKIT_F32_MAX};
    }

    current.ChildOffset = params.ChildOffset;
    current.SelfOffset = params.SelfOffset;
    current.ChildGap = params.ChildGap;
    current.Color = params.Color;
    current.Direction = params.Direction;
    current.ChildAlignment = params.Alignment;
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
        current.Size[i] = params.Sizing[i].Type == LayoutSizing_Fixed ? params.Sizing[i].Size : 0.f;
        current.Sizing[i] = params.Sizing[i].Type;
        current.MaxSize[i] = params.Sizing[i].Max;
        current.MinSize[i] = params.Sizing[i].Min;
        // bc fits get clamped in the fit pass
        if (current.Sizing[i] != LayoutSizing_Fit)
            current.Size[i] = Math::Clamp(current.Size[i], current.MinSize[i], current.MaxSize[i]);
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
    current.SelfAlignment = parent.ChildAlignment;
    current.SelfOverflow = parent.ChildOverflow;

    current.SelfOffset = params.Offset;
    current.Color = params.Color;
    current.Text = TKit::String{text.GetData(), text.GetSize()};
    current.Font = params.Font == NullHandle ? m_Specs.Font : params.Font;
    current.TextMode = params.Mode;

    const FontData &fdata = Resources::GetFontData(current.Font);

    const f32 fs = params.FontSize;
    current.FontSize = fs;
    current.Size = fs * fdata.ComputeTextSize(text);

    current.MinSize[0] = 0.f; // fs * fdata.ComputeTextMinimumWidth(text);
    current.MinSize[1] = 0.f;

    current.MaxSize = f32v2{TKIT_F32_MAX};
}

void Layout::fitPass(const u32 axis)
{
    for (const u32 p : m_ReversedBreadth)
    {
        LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");
        if (parent.Type != LayoutElement_Panel || parent.Sizing[axis] != LayoutSizing_Fit)
            continue;

        const LayoutDirection dir = parent.Direction;
        f32 &psize = parent.Size[axis];

        f32 childMinSizeTotal = 0.f;

        f32 &pmnsize = parent.MinSize[axis];
        const f32 pmxsize = parent.MaxSize[axis];
        for (const u32 c : parent.Children)
        {
            const LayoutElement &child = m_Elements[c];
            const f32 csize = child.Size[axis];
            const f32 cmnsize = child.MinSize[axis];

            TKIT_ASSERT(child.Parent != TKIT_U32_MAX, "[ONYX][LAYOUT] Only the root node can have no parent");
            if (dir == axis)
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

        if (dir == axis)
            psize += parent.ChildGap * (parent.Children.GetSize() - 1);

        psize = Math::Clamp(psize, pmnsize, pmxsize);
    }
}

void Layout::growShrinkPass(const u32 axis)
{
    for (const u32 p : m_Breadth)
    {
        const LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");

        const LayoutDirection dir = parent.Direction;
        const f32 padding = parent.Padding[2 * axis] + parent.Padding[2 * axis + 1];
        f32 remainingSize = parent.Size[axis] - padding;
        if (dir == axis)
        {
            remainingSize -= parent.ChildGap * (parent.Children.GetSize() - 1);
            TKit::StackArray<u32> toGrow{};
            toGrow.Reserve(m_Elements.GetSize());
            TKit::StackArray<u32> toShrink{};
            toShrink.Reserve(m_Elements.GetSize());

            for (const u32 c : parent.Children)
            {
                const LayoutElement &child = m_Elements[c];
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
                f32 &csize = child.Size[axis];
                const f32 mxsize = child.MaxSize[axis];
                const f32 mnsize = child.MinSize[axis];
                if ((child.Type == LayoutElement_Panel && child.Sizing[axis] == LayoutSizing_Grow) ||
                    (axis == 0 && child.Type == LayoutElement_Text))
                    csize = Math::Clamp(remainingSize, mnsize, mxsize);
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
    for (const u32 p : m_Breadth)
    {
        const LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");

        const LayoutDirection dir = parent.Direction;

        const f32 cgap = parent.ChildGap;

        for (u32 axis = 0; axis < 2; ++axis)
        {

            const auto computeChildSize = [this, &parent, axis, cgap, dir] {
                f32 totalChildSize = 0.f;
                for (const u32 c : parent.Children)
                    totalChildSize += m_Elements[c].Size[axis];
                totalChildSize += cgap * (parent.Children.GetSize() - 1);
                return dir == LayoutDirection_Horizontal ? -totalChildSize : totalChildSize;
            };

            const f32 psize = parent.Size[axis];
            const f32 ppos = parent.Position[axis];
            const f32 coffset = parent.ChildOffset[axis];

            const Alignment salg = parent.SelfAlignment[axis];
            const Alignment calg = parent.ChildAlignment[axis];

            const f32 p0 = parent.Padding[2 * axis];
            const f32 p1 = parent.Padding[2 * axis + 1];

            const f32 clmn = parent.ClipMin[axis];
            const f32 clmx = parent.ClipMax[axis];

            f32 pmn;
            f32 pmx;

            f32 poffset = ppos + coffset;
            // haha lots of ifs
            if (salg == Alignment_Canonical)
            {
                pmn = ppos;
                pmx = ppos + psize;
                if (calg == Alignment_Canonical)
                    poffset += p0;
                else if (calg == Alignment_Mirrored)
                    poffset += psize - p1;
                else if (dir == axis)
                    poffset += 0.5f * (computeChildSize() + psize) + p0 - p1;
                else
                    poffset += p0 - p1;
            }
            else if (salg == Alignment_Mirrored)
            {
                pmn = ppos - psize;
                pmx = ppos;
                if (calg == Alignment_Canonical)
                    poffset += p0 - psize;
                else if (calg == Alignment_Mirrored)
                    poffset -= p1;
                else if (dir == axis)
                    poffset += 0.5f * (computeChildSize() - psize) + p0 - p1;
                else
                    poffset += p0 - p1;
            }
            else
            {
                pmn = ppos - 0.5f * psize;
                pmx = ppos + 0.5f * psize;
                if (calg == Alignment_Canonical)
                    poffset += p0 - 0.5f * psize;
                else if (calg == Alignment_Mirrored)
                    poffset += 0.5f * psize - p1;
                else if (dir == axis)
                    poffset += 0.5f * computeChildSize() + p0 - p1;
                else
                    poffset += p0 - p1;
            }

            for (const u32 c : parent.Children)
            {
                LayoutElement &child = m_Elements[c];

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

                const f32 csize = child.Size[axis];
                f32 &cpos = child.Position[axis];

                cpos += poffset + child.SelfOffset[axis];
                if (calg == Alignment_Center)
                {
                    if (dir == axis)
                        cpos += 0.5f * (dir == LayoutDirection_Horizontal ? csize : -csize);
                    else if (salg == Alignment_Canonical)
                        cpos += 0.5f * psize;
                    else if (salg == Alignment_Mirrored)
                        cpos -= 0.5f * psize;
                }

                if (dir == axis)
                {
                    if (calg == Alignment_Canonical || (calg == Alignment_Center && dir == LayoutDirection_Horizontal))
                        poffset += csize + cgap;
                    else
                        poffset -= csize + cgap;
                }
            }
        }
    }
}

void Layout::Compile()
{
    TKIT_ASSERT(m_Stack.IsEmpty(), "[ONYX][LAYOUT] Trying to compile a layout that has {} open nodes!",
                m_Stack.GetSize());
    m_ElementInfo.Clear();
    fitPass(0);
    growShrinkPass(0);
    wrapText();
    fitPass(1);
    growShrinkPass(1);
    positionPass();
    for (const LayoutElement &elm : m_Elements)
    {
        if (TKit::ApproachesZero(elm.Color.rgba[3]) ||
            (elm.Type != LayoutElement_Text &&
             (TKit::ApproachesZero(elm.Size[0]) || TKit::ApproachesZero(elm.Size[1]))))
            continue;

        LayoutElementInfo &info = m_ElementInfo.Append();
        info.Color = elm.Color;
        info.Position = elm.Position;
        info.Alignment = elm.SelfAlignment;
        info.ClipMin = elm.ClipMin;
        info.ClipMax = elm.ClipMax;
        if (elm.Type == LayoutElement_Text)
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

    m_Elements.Clear();
    m_Breadth.Clear();
    m_ReversedBreadth.Clear();
}
} // namespace Onyx
