#include "pch.hpp"
#include "onyx/ui.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx
{
void Layout::BeginPanel(const PanelParameters &params)
{
    const u32 c = m_Elements.GetSize();
    LayoutElement &current = m_Elements.Append();

    current.Parent = m_Stack.IsEmpty() ? TKIT_U32_MAX : m_Stack.GetBack();
    m_Stack.Append(c);
    if (current.Parent != TKIT_U32_MAX)
    {
        LayoutElement &parent = m_Elements[current.Parent];
        if (parent.Children.IsEmpty())
            m_Breadth.Append(current.Parent);
        parent.Children.Append(c);
        current.SelfAlignment = parent.ChildAlignment;
    }
    else
        current.SelfAlignment = params.Alignment;

    current.ChildGap = params.ChildGap;
    current.Color = params.Color;
    current.Direction = params.Direction;
    current.ChildAlignment = params.Alignment;
    for (u32 i = 0; i < 2; ++i)
    {
        current.Size[i] = params.Sizing[i].Type == LayoutSizing_Fixed ? params.Sizing[i].Size : 0.f;
        current.MaxSize[i] = params.Sizing[i].Max;
        current.MinSize[i] = params.Sizing[i].Min;
        current.Sizing[i] = params.Sizing[i].Type;
    }
    current.Padding = params.Padding;
    current.CornerRadius = params.CornerRadius;
}

void Layout::EndPanel()
{
    TKIT_ASSERT(!m_Stack.IsEmpty(), "[ONYX][UI] Begin()/End() Mismatch! Every Begin() must be matched with an End()");

    const u32 c = m_Stack.GetBack();
    if (!m_Elements[c].Children.IsEmpty())
        m_ReversedBreadth.Append(c);

    m_Stack.Pop();
}

void Layout::fitPass(const u32 axis)
{
    for (const u32 p : m_ReversedBreadth)
    {
        LayoutElement &parent = m_Elements[p];
        TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");
        if (parent.Sizing[axis] != LayoutSizing_Fit)
            continue;

        const LayoutDirection dir = parent.Direction;
        f32 &psize = parent.Size[axis];
        for (const u32 c : parent.Children)
        {
            const LayoutElement &child = m_Elements[c];
            const f32 csize = child.Size[axis];
            TKIT_ASSERT(child.Parent != TKIT_U32_MAX, "[ONYX][LAYOUT] Only the root node can have no parent");
            if (dir == axis)
                psize += csize;
            else
                psize = Math::Max(psize, csize);
        }

        const f32 padding = parent.Padding[2 * axis] + parent.Padding[2 * axis + 1];
        psize += padding;
        if (dir == axis)
        {
            const f32 childGap = parent.ChildGap * (parent.Children.GetSize() - 1);
            psize += childGap;
        }
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

            for (const u32 c : parent.Children)
            {
                const LayoutElement &child = m_Elements[c];
                const f32 csize = child.Size[axis];
                remainingSize -= csize;
                if (child.Sizing[axis] == LayoutSizing_Grow && csize < child.MaxSize[axis])
                    toGrow.Append(c);
            }

            while (!toGrow.IsEmpty() && remainingSize > 0.f)
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
                    if (csize + toAdd >= child.MaxSize[axis])
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
        }
        else
            for (const u32 c : parent.Children)
            {
                LayoutElement &child = m_Elements[c];
                f32 &csize = child.Size[axis];
                if (child.Sizing[axis] == LayoutSizing_Grow)
                    csize = Math::Min(csize + remainingSize, child.MaxSize[axis]);
            }
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
            const f32 psize = parent.Size[axis];
            const f32 ppos = parent.Position[axis];

            const auto computeChildSize = [this, &parent, axis, cgap, dir] {
                f32 totalChildSize = 0.f;
                for (const u32 c : parent.Children)
                    totalChildSize += m_Elements[c].Size[axis];
                totalChildSize += cgap * (parent.Children.GetSize() - 1);
                return dir == LayoutDirection_Horizontal ? -totalChildSize : totalChildSize;
            };
            const Alignment salg = parent.SelfAlignment[axis];
            const Alignment calg = parent.ChildAlignment[axis];

            const f32 p0 = parent.Padding[2 * axis];
            const f32 p1 = parent.Padding[2 * axis + 1];

            f32 poffset = 0.f;
            // haha lots of ifs
            if (salg == Alignment_Left)
            {
                if (calg == Alignment_Left)
                    poffset = ppos + p0;
                else if (calg == Alignment_Right)
                    poffset = ppos + psize - p1;
                else if (dir == axis)
                    poffset = ppos + 0.5f * (computeChildSize() + psize) + p0 - p1;
                else
                    poffset = ppos + p0 - p1;
            }
            else if (salg == Alignment_Right)
            {
                if (calg == Alignment_Left)
                    poffset = ppos - psize + p0;
                else if (calg == Alignment_Right)
                    poffset = ppos - p1;
                else if (dir == axis)
                    poffset = ppos + 0.5f * (computeChildSize() - psize) + p0 - p1;
                else
                    poffset = ppos + p0 - p1;
            }
            else
            {
                if (calg == Alignment_Left)
                    poffset = ppos + 0.5f * psize + p0;
                else if (calg == Alignment_Right)
                    poffset = ppos + 0.5f * psize - p1;
                else if (dir == axis)
                    poffset = ppos + 0.5f * computeChildSize() + p0 - p1;
                else
                    poffset = ppos + p0 - p1;
            }

            for (const u32 c : parent.Children)
            {
                LayoutElement &child = m_Elements[c];

                const f32 csize = child.Size[axis];
                f32 &cpos = child.Position[axis];
                if (calg == Alignment_Center)
                {
                    if (dir == axis)
                        cpos += poffset + 0.5f * (dir == LayoutDirection_Horizontal ? csize : -csize);
                    else if (salg == Alignment_Left)
                        cpos += poffset + 0.5f * psize;
                    else if (salg == Alignment_Right)
                        cpos += poffset - 0.5f * psize;
                    else
                        cpos += poffset;
                }
                else
                    cpos += poffset;

                if (dir == axis)
                {
                    if (calg == Alignment_Left || (calg == Alignment_Center && dir == LayoutDirection_Horizontal))
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
    fitPass(1);
    growShrinkPass(0);
    growShrinkPass(1);
    positionPass();
    for (const LayoutElement &elm : m_Elements)
    {
        if (elm.Color.rgba[3] == 0.f)
            continue;
        LayoutElementInfo &info = m_ElementInfo.Append();
        info.Color = elm.Color;
        info.Position = elm.Position;
        info.Size = elm.Size - 2.f * elm.CornerRadius;
        info.Radius = elm.CornerRadius;
        info.Alignment = elm.SelfAlignment;
        if (elm.CornerRadius == 0.f)
        {
            info.Geo = Geometry_Static;
            info.Handle = m_Square;
        }
        else
        {
            info.Geo = Geometry_Parametric;
            info.Handle = m_RoundedSquare;
        }
    }

    m_Elements.Clear();
    m_Breadth.Clear();
    m_ReversedBreadth.Clear();
}

} // namespace Onyx
