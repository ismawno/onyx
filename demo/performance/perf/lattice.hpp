#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/core/math.hpp"
#include "onyx/rendering/context.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/serialization/yaml/serialize.hpp"
#include "tkit/multiprocessing/for_each.hpp"
#include "tkit/profiling/macros.hpp"

namespace Onyx::Demo
{
TKIT_YAML_SERIALIZE_DECLARE_ENUM(Shape)
enum class Shape : u8
{
    Triangle,
    Square,
    NGon,
    Polygon,
    Circle,
    Cube,
    Cylinder,
    Sphere,
    ImportedStatic,
};
struct ShapeSettings
{
    TKIT_YAML_SERIALIZE_DECLARE(ShapeSettings)
    Shape Shape = Shape::Triangle;
    CircleOptions CircleOptions{};
    TKit::StaticArray8<f32v2> PolygonVertices{f32v2{0.5f, -0.3f}, f32v2{0.f, 0.3f}, f32v2{-0.5f, -0.3f}};
    std::string MeshPath{};

    u32 NGonSides = 3;
    u32 SphereRings = 32;
    u32 SphereSectors = 64;
    u32 CylinderSides = 64;
};
template <Dimension D> struct Lattice
{
    TKIT_YAML_SERIALIZE_DECLARE(Lattice)

    void Render(RenderContext<D> *context) const;

    void StaticMesh(RenderContext<D> *context, Mesh mesh) const;
    void Circle(RenderContext<D> *context, const CircleOptions &options) const;

    template <typename F> void Run(F &&func) const
    {
        TKit::ITaskManager *tm = Core::GetTaskManager();

        if constexpr (D == D2)
        {
            const u32 size = LatticeDims[0] * LatticeDims[1];
            const auto fn = [this, &func](const u32 start, const u32 end) {
                TKIT_PROFILE_NSCOPE("Onyx::Demo::Lattice");

                const f32v<D> offset = -0.5f * Separation * f32v<D>{LatticeDims - 1u};
                for (u32 i = start; i < end; ++i)
                {
                    const u32 ix = i / LatticeDims[1];
                    const u32 iy = i % LatticeDims[1];
                    const f32 x = Separation * static_cast<f32>(ix);
                    const f32 y = Separation * static_cast<f32>(iy);

                    const f32v2 pos = f32v2{x, y} + offset;
                    std::forward<F>(func)(pos);
                }
            };

            TKit::FixedArray<Task, MaxTasks> tasks{};
            const u32 partitions = Tasks; // Unfortunate naming
            TKit::BlockingForEach(*tm, 0u, size, tasks.begin(), partitions, fn);

            const u32 tcount = (partitions - 1) >= MaxTasks ? MaxTasks : (partitions - 1);
            for (u32 i = 0; i < tcount; ++i)
                tm->WaitUntilFinished(tasks[i]);
        }
        else
        {
            const u32 size = LatticeDims[0] * LatticeDims[1] * LatticeDims[2];
            const auto fn = [this, &func](const u32 start, const u32 end) {
                TKIT_PROFILE_NSCOPE("Onyx::Demo::Lattice");

                const u32 yz = LatticeDims[1] * LatticeDims[2];
                const f32v<D> offset = Transform.Translation - 0.5f * Separation * f32v<D>{LatticeDims - 1u};
                for (u32 i = start; i < end; ++i)
                {
                    const u32 ix = i / yz;
                    const u32 j = ix * yz;
                    const u32 iy = (i - j) / LatticeDims[2];
                    const u32 iz = (i - j) % LatticeDims[2];
                    const f32 x = Separation * static_cast<f32>(ix);
                    const f32 y = Separation * static_cast<f32>(iy);
                    const f32 z = Separation * static_cast<f32>(iz);
                    const f32v3 pos = f32v3{x, y, z} + offset;
                    std::forward<F>(func)(pos);
                }
            };

            TKit::FixedArray<Task, MaxTasks> tasks{};
            const u32 partitions = Tasks; // Unfortunate naming
            TKit::BlockingForEach(*tm, 0u, size, tasks.begin(), partitions, fn);

            const u32 tcount = (partitions - 1) >= MaxTasks ? MaxTasks : (partitions - 1);
            for (u32 i = 0; i < tcount; ++i)
                tm->WaitUntilFinished(tasks[i]);
        }
    }

    Transform<D> Transform{};
    Onyx::Color Color = Onyx::Color::WHITE;

    u32v<D> LatticeDims{10};

    TKIT_YAML_SERIALIZE_GROUP_BEGIN("Optionals", "--skip-if-missing")
    f32 Separation = 2.5f;
    u32 Tasks = 1;
    TKIT_YAML_SERIALIZE_GROUP_END()
};
} // namespace Onyx::Demo
