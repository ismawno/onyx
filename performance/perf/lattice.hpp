#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/glm.hpp"
#include "onyx/rendering/render_context.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/serialization/yaml/serialize.hpp"
#include "tkit/multiprocessing/for_each.hpp"
#include "tkit/profiling/macros.hpp"

namespace Onyx::Perf
{
TKIT_YAML_SERIALIZE_DECLARE_ENUM(Shapes2)
TKIT_YAML_SERIALIZE_DECLARE_ENUM(Shapes3)
enum class Shapes2 : u8
{
    Triangle = 0,
    Square,
    NGon,
    Polygon,
    Circle,
    Stadium,
    RoundedSquare,
    Mesh,
};
enum class Shapes3 : u8
{
    Triangle = 0,
    Square,
    NGon,
    Polygon,
    Circle,
    Stadium,
    RoundedSquare,
    Mesh,
    Cube,
    Cylinder,
    Sphere,
    Capsule,
    RoundedCube,
};

template <Dimension D> struct Shapes;

template <> struct Shapes<D2>
{
    using Type = Shapes2;
};
template <> struct Shapes<D3>
{
    using Type = Shapes3;
};
template <Dimension D> using ShapeType = Shapes<D>::Type;

template <Dimension D> struct Lattice
{
    TKIT_REFLECT_DECLARE(Lattice)
    TKIT_YAML_SERIALIZE_DECLARE(Lattice)

    void Render(RenderContext<D> *p_Context) const noexcept;

    template <typename F> void Run(F &&p_Func) const noexcept
    {
        if (Multithread)
            RunMultiThread(std::forward<F>(p_Func));
        else
            RunSingleThread(std::forward<F>(p_Func));
    }
    template <typename F> void RunSingleThread(F &&p_Func) const noexcept
    {
        TKIT_PROFILE_NSCOPE("Onyx::Perf::Lattice");
        const fvec<D> midPoint = 0.5f * Separation * fvec<D>{LatticeDims - 1u};
        for (u32 i = 0; i < LatticeDims.x; ++i)
        {
            const f32 x = static_cast<f32>(i) * Separation;
            for (u32 j = 0; j < LatticeDims.y; ++j)
            {
                const f32 y = static_cast<f32>(j) * Separation;
                if constexpr (D == D2)
                {
                    const fvec2 pos = fvec2{x, y} - midPoint;
                    std::forward<F>(p_Func)(pos);
                }
                else
                    for (u32 k = 0; k < LatticeDims.z; ++k)
                    {
                        const f32 z = static_cast<f32>(k) * Separation;
                        const fvec3 pos = fvec3{x, y, z} - midPoint;
                        std::forward<F>(p_Func)(pos);
                    }
            }
        }
    }

    template <typename F> void RunMultiThread(F &&p_Func) const noexcept
    {
        TKIT_PROFILE_NSCOPE("Onyx::Perf::Lattice");
        const fvec<D> midPoint = 0.5f * Separation * fvec<D>{LatticeDims - 1u};
        TKit::ITaskManager *tm = Core::GetTaskManager();
        using Task = TKit::Ref<TKit::Task<void>>;
        if constexpr (D == D2)
        {
            const u32 size = LatticeDims.x * LatticeDims.y;
            const auto fn = [this, &midPoint, &p_Func](const u32 p_Start, const u32 p_End, const u32) {
                TKIT_PROFILE_NSCOPE("Onyx::Perf::Task");
                for (u32 i = p_Start; i < p_End; ++i)
                {
                    const u32 ix = i / LatticeDims.y;
                    const u32 iy = i % LatticeDims.y;
                    const f32 x = Separation * static_cast<f32>(ix);
                    const f32 y = Separation * static_cast<f32>(iy);

                    const fvec2 pos = fvec2{x, y} - midPoint;
                    std::forward<F>(p_Func)(pos);
                }
            };
            TKit::Array<Task, ONYX_MAX_THREADS> tasks{};
            TKit::ForEachMainThreadLead(*tm, 0u, size, tasks.begin(), Tasks, fn);
            for (u32 i = 0; i < Tasks - 1; ++i)
                tasks[i]->WaitUntilFinished();
        }
        else
        {
            const u32 size = LatticeDims.x * LatticeDims.y * LatticeDims.z;
            const u32 yz = LatticeDims.y * LatticeDims.z;
            const auto fn = [this, yz, &midPoint, &p_Func](const u32 p_Start, const u32 p_End, const u32) {
                TKIT_PROFILE_NSCOPE("Onyx::Perf::Task");
                for (u32 i = p_Start; i < p_End; ++i)
                {
                    const u32 ix = i / yz;
                    const u32 j = ix * yz;
                    const u32 iy = (i - j) / LatticeDims.z;
                    const u32 iz = (i - j) % LatticeDims.z;
                    const f32 x = Separation * static_cast<f32>(ix);
                    const f32 y = Separation * static_cast<f32>(iy);
                    const f32 z = Separation * static_cast<f32>(iz);
                    const fvec3 pos = fvec3{x, y, z} - midPoint;
                    std::forward<F>(p_Func)(pos);
                }
            };
            TKit::Array<Task, ONYX_MAX_THREADS> tasks{};
            TKit::ForEachMainThreadLead(*tm, 0u, size, tasks.begin(), Tasks, fn);
            for (u32 i = 0; i < Tasks - 1; ++i)
                tasks[i]->WaitUntilFinished();
        }
    }

    Transform<D> Transform{};
    ShapeType<D> Shape = ShapeType<D>::Triangle;
    Onyx::Color Color = Onyx::Color::WHITE;

    TKIT_YAML_SERIALIZE_IGNORE_BEGIN()
    Mesh<D> Mesh{};
    TKIT_YAML_SERIALIZE_IGNORE_END()

    uvec<D> LatticeDims{10};

    TKIT_YAML_SERIALIZE_GROUP_BEGIN("Optionals", "--skip-if-missing")
    std::string MeshPath{};
    CircleOptions CircleOptions{};
    fvec<D> ShapeSize{1.f};
    TKit::StaticArray<fvec2, ONYX_MAX_POLYGON_VERTICES> Vertices{{0.5f, -0.3f}, {0.f, 0.3f}, {-0.5f, -0.3f}};
    u32 NGonSides = 3;
    f32 Diameter = 1.f;
    f32 Length = 1.f;
    Resolution Res = Resolution::Medium;

    f32 Separation = 2.5f;
    u32 Tasks = 1;
    TKIT_YAML_SERIALIZE_GROUP_END()
    bool Multithread = false;
};
} // namespace Onyx::Perf
