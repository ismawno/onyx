#include "pch.hpp"
#include "onyx/ui.hpp"

namespace Onyx
{
void Layout::BeginPanel(const PanelParameters &params)
{
    TKIT_ASSERT(params.Sizing[0].Type != LayoutSizing_Grow && params.Sizing[1].Type != LayoutSizing_Grow,
                "[ONYX][LAYOUT] The root element of a layout cannot have a grow sizing");

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
        for (const u32 c : parent.Children)
        {
            const LayoutElement &child = m_Elements[c];
            TKIT_ASSERT(child.Parent != TKIT_U32_MAX, "[ONYX][LAYOUT] Only the root node can have no parent");
            if (dir == axis)
                parent.Size[axis] += child.Size[axis];
            else
                parent.Size[axis] = Math::Max(parent.Size[axis], child.Size[axis]);
        }

        const f32 padding = parent.Padding[2 * axis] + parent.Padding[2 * axis + 1];
        parent.Size[axis] += padding;
        if (dir == axis)
        {
            const f32 childGap = parent.ChildGap * (parent.Children.GetSize() - 1);
            parent.Size[axis] += childGap;
        }
    }
}

// void Layout::growPass()
// {
//     for (const u32 p : m_Breadth)
//     {
//         const LayoutElement &parent = m_Elements[p];
//         TKIT_ASSERT(!parent.Children.IsEmpty(), "[ONYX][LAYOUT] Only non-leaf nodes allowed in traversal");
//
//         const LayoutDirection dir = parent.Direction;
//         for (u32 axis = 0; axis < 2; ++axis)
//         {
//             f32 remainingSize = parent.Size[axis];
//         }
//     }
// }

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
                if (calg == Alignment_Center)
                {
                    if (dir == axis)
                        child.Position[axis] += poffset + 0.5f * (dir == LayoutDirection_Horizontal ? csize : -csize);
                    else if (salg == Alignment_Left)
                        child.Position[axis] += poffset + 0.5f * psize;
                    else if (salg == Alignment_Right)
                        child.Position[axis] += poffset - 0.5f * psize;
                    else
                        child.Position[axis] += poffset;
                }
                else
                    child.Position[axis] += poffset;

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
