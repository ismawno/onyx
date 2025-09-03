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

    void Render(RenderContext<D> *p_Context) const;

    template <typename F> void Run(F &&p_Func) const
    {
        TKit::ITaskManager *tm = Core::GetTaskManager();

        if constexpr (D == D2)
        {
            const u32 size = LatticeDims.x * LatticeDims.y;
            const auto fn = [this, &p_Func](const u32 p_Start, const u32 p_End) {
                TKIT_PROFILE_NSCOPE("Onyx::Perf::Lattice");
                const Lattice<D> lattice = *this;

                const fvec<D> offset = -0.5f * Separation * fvec<D>{LatticeDims - 1u};
                for (u32 i = p_Start; i < p_End; ++i)
                {
                    const u32 ix = i / LatticeDims.y;
                    const u32 iy = i % LatticeDims.y;
                    const f32 x = Separation * static_cast<f32>(ix);
                    const f32 y = Separation * static_cast<f32>(iy);

                    const fvec2 pos = fvec2{x, y} + offset;
                    std::forward<F>(p_Func)(pos, lattice);
                }
            };
            TKit::Array<TKit::Task<> *, ONYX_MAX_TASKS> tasks{};
            const u32 partitions = Tasks; // Unfortunate naming
            TKit::BlockingForEach(*tm, 0u, size, tasks.begin(), partitions, fn);

            const u32 tcount = (partitions - 1) >= ONYX_MAX_TASKS ? ONYX_MAX_TASKS : (partitions - 1);
            for (u32 i = 0; i < tcount; ++i)
            {
                tasks[i]->WaitUntilFinished();
                tm->DestroyTask(tasks[i]);
            }
        }
        else
        {
            const u32 size = LatticeDims.x * LatticeDims.y * LatticeDims.z;
            const auto fn = [this, &p_Func](const u32 p_Start, const u32 p_End) {
                TKIT_PROFILE_NSCOPE("Onyx::Perf::Lattice");

                const Lattice<D> lattice = *this;
                const u32 yz = LatticeDims.y * LatticeDims.z;
                const fvec<D> offset = Transform.Translation - 0.5f * Separation * fvec<D>{LatticeDims - 1u};
                for (u32 i = p_Start; i < p_End; ++i)
                {
                    const u32 ix = i / yz;
                    const u32 j = ix * yz;
                    const u32 iy = (i - j) / LatticeDims.z;
                    const u32 iz = (i - j) % LatticeDims.z;
                    const f32 x = Separation * static_cast<f32>(ix);
                    const f32 y = Separation * static_cast<f32>(iy);
                    const f32 z = Separation * static_cast<f32>(iz);
                    const fvec3 pos = fvec3{x, y, z} + offset;
                    std::forward<F>(p_Func)(pos, lattice);
                }
            };
            TKit::Array<TKit::Task<> *, ONYX_MAX_TASKS> tasks{};
            const u32 partitions = Tasks; // Unfortunate naming
            TKit::BlockingForEach(*tm, 0u, size, tasks.begin(), partitions, fn);

            const u32 tcount = (partitions - 1) >= ONYX_MAX_TASKS ? ONYX_MAX_TASKS : (partitions - 1);
            for (u32 i = 0; i < tcount; ++i)
            {
                tasks[i]->WaitUntilFinished();
                tm->DestroyTask(tasks[i]);
            }
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
    PolygonVerticesArray Vertices{{0.5f, -0.3f}, {0.f, 0.3f}, {-0.5f, -0.3f}};
    u32 NGonSides = 3;
    f32 Diameter = 1.f;
    f32 Length = 1.f;
    Resolution Res = Resolution::Medium;

    f32 Separation = 2.5f;
    u32 Tasks = 1;
    TKIT_YAML_SERIALIZE_GROUP_END()
};
} // namespace Onyx::Perf
