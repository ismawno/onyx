#include "core/pch.hpp"
#include "onyx/draw/data.hpp"
#include <tiny_obj_loader.h>

namespace ONYX
{
template <Dimension D> IndexVertexData<D> Load(const std::string_view p_Path) noexcept
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    KIT_ASSERT_RETURNS(tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, p_Path.data()), true,
                       "Failed to load model: {}", err + warn);

    HashMap<Vertex<D>, Index> uniqueVertices;
    IndexVertexData<D> buffers;

    buffers.Vertices.reserve(attrib.vertices.size() / 3);
    buffers.Indices.reserve(shapes.size() * 3);

    for (const auto &shape : shapes)
        for (const auto &index : shape.mesh.indices)
        {
            Vertex<D> vertex{};
            for (Index i = 0; i < D; ++i)
                vertex.Position[i] = attrib.vertices[3 * index.vertex_index + i];
            if constexpr (D == D3)
                for (Index i = 0; i < 3; ++i)
                    vertex.Normal[i] = attrib.normals[3 * index.normal_index + i];

            if (!uniqueVertices.contains(vertex))
            {
                uniqueVertices[vertex] = static_cast<Index>(uniqueVertices.size());
                buffers.Vertices.push_back(vertex);
            }
            buffers.Indices.push_back(uniqueVertices[vertex]);
        }
    return buffers;
}

template IndexVertexData<D2> Load(const std::string_view p_Path) noexcept;
template IndexVertexData<D3> Load(const std::string_view p_Path) noexcept;

template struct IndexVertexData<D2>;
template struct IndexVertexData<D3>;

} // namespace ONYX