#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/glm.hpp"
#include "onyx/rendering/render_context.hpp"

namespace Onyx::Perf
{
enum Shapes2 : u8
{
    Triangle,
    Square,
    NGon,
    Polygon,
    Circle,
    Stadium,
    RoundedSquare,
};
enum Shapes3 : u8
{
};
template <Dimension D> struct Lattice
{
    void Render(RenderContext<D> *p_Context) noexcept;

    template <typename F> void Run(F &&p_Func) noexcept
    {
        if (Multithread)
            RunMultiThread(std::forward<F>(p_Func));
        else
            RunSingleThread(std::forward<F>(p_Func));
    }
    template <typename F> void RunSingleThread(F &&p_Func) noexcept
    {
        const f32 midPoint = 0.5f * Separation * fvec<D>{Dimensions - 1u};
        for (u32 i = 0; i < Dimensions.x; ++i)
        {
            const f32 x = static_cast<f32>(i) * Separation;
            for (u32 j = 0; j < Dimensions.y; ++j)
            {
                const f32 y = static_cast<f32>(j) * Separation;
                if constexpr (D == D2)
                {
                    const fvec2 pos = fvec2{x, y} - midPoint;
                    std::forward<F>(p_Func)(pos);
                }
                else
                    for (u32 k = 0; k < Dimensions.z; ++k)
                    {
                        const f32 z = static_cast<f32>(k) * Separation;
                        const fvec3 pos = fvec3{x, y, z};
                        std::forward<F>(p_Func)(pos);
                    }
            }
        }
    }

    template <typename F> void RunMultiThread(F &&) noexcept
    {
    }
    uvec<D> Dimensions{10};
    f32 Separation = 1.f;
    bool Multithread = false;
};
} // namespace Onyx::Perf
