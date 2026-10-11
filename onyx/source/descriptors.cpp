#include "pch.hpp"
#include "onyx/definitions.hpp"
#include "descriptors.hpp"
#include "graph/descriptor.hpp"
#include "conversion.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/container/stack_array.hpp"

namespace Onyx::Descriptors
{
struct DescriptorData
{
    ten<Graph::DescriptorSetLayout, D_Count, RenderPass_Count> Layouts{};
    TKit::FixedArray<Graph::DescriptorSetLayout, StandalonePass_Count> StandaloneLayouts{};
};

static TKit::Storage<DescriptorData> s_DescriptorData{};

constexpr u32 Dim2 = 0;
constexpr u32 Dim3 = 1;

static void createDescriptorData()
{
    // TODO(Isma): There are several flags with the unused while pending bit. revisit if those are really necessary

    using Binding = Graph::DescriptorBinding;
    using BindingArray = TKit::StaticArray16<Binding>;

    constexpr Graph::DescriptorType buffer = Graph::Descriptor_StorageBuffer;
    constexpr Graph::DescriptorType sampler = Graph::Descriptor_Sampler;
    constexpr Graph::DescriptorType sampledImage = Graph::Descriptor_SampledImage;
    constexpr Graph::DescriptorType combined = Graph::Descriptor_CombinedImageSampler;
    constexpr Graph::DescriptorType storageImage = Graph::Descriptor_StorageImage;

    constexpr Graph::ShaderStageFlags compute = Graph::ShaderStageFlag_Compute;
    constexpr Graph::ShaderStageFlags vertex = Graph::ShaderStageFlag_Vertex;
    constexpr Graph::ShaderStageFlags fragment = Graph::ShaderStageFlag_Fragment;

    // bind unused is disabled for now
    constexpr Graph::DescriptorBindingFlags pbound = Graph::DescriptorBindingFlag_PartiallyBound;

    BindingArray flat{};
    flat.Append(Binding::Create(ONYX_INSTANCES_BINDING_POINT, buffer, vertex | fragment));
    flat.Append(Binding::Create(ONYX_SAMPLERS_BINDING_POINT, sampler, fragment, ONYX_MAX_SAMPLERS, pbound));
    flat.Append(Binding::Create(ONYX_TEXTURES_BINDING_POINT, sampledImage, fragment, ONYX_MAX_TEXTURES, pbound));
    flat.Append(Binding::Create(ONYX_TEXTURE_OFFSETS_BINDING_POINT, buffer, fragment));
    flat.Append(Binding::Create(ONYX_BOUNDS_BINDING_POINT, buffer, vertex));

    s_DescriptorData->Layouts[Dim2][RenderPass_Flat] = Graph::DescriptorSetLayout_Create(flat);
    s_DescriptorData->Layouts[Dim3][RenderPass_Flat] = Graph::DescriptorSetLayout_Create(flat);

    // flat is also the common prefix of the shaded bindings
    BindingArray shaded = flat;
    shaded.Append(Binding::Create(ONYX_MATERIALS_BINDING_POINT, buffer, fragment));
    shaded.Append(Binding::Create(ONYX_POINT_LIGHTS_BINDING_POINT, buffer, fragment));
    shaded.Append(Binding::Create(ONYX_DIRECTIONAL_LIGHTS_BINDING_POINT, buffer, fragment));
    shaded.Append(Binding::Create(ONYX_SHADOW_SAMPLER_BINDING_POINT, sampler, fragment));
    shaded.Append(Binding::Create(ONYX_SHADOW_COMPARE_SAMPLER_BINDING_POINT, sampler, fragment));

    // we perform a copy to not modify shaded
    BindingArray shaded2 = shaded;
    shaded2.Append(Binding::Create(ONYX_SHADOW_MAPS_BINDING_POINT, sampledImage, fragment,
                                   ONYX_MAX_RAY_MARCH_AND_OCCLUSION_MAP_SIZE, pbound));
    s_DescriptorData->Layouts[Dim2][RenderPass_Shaded] = Graph::DescriptorSetLayout_Create(shaded2);

    shaded.Append(
        Binding::Create(ONYX_POINT_MAPS_BINDING_POINT, sampledImage, fragment, ONYX_MAX_TEXTURE_MAPS, pbound));
    shaded.Append(
        Binding::Create(ONYX_DIRECTIONAL_MAPS_BINDING_POINT, sampledImage, fragment, ONYX_MAX_TEXTURE_MAPS, pbound));
    shaded.Append(Binding::Create(ONYX_SPOT_MAPS_BINDING_POINT, sampledImage, fragment, ONYX_MAX_TEXTURE_MAPS, pbound));
    shaded.Append(Binding::Create(ONYX_SPOT_LIGHTS_BINDING_POINT, buffer, fragment));
    s_DescriptorData->Layouts[Dim3][RenderPass_Shaded] = Graph::DescriptorSetLayout_Create(shaded);

    BindingArray shadow{};
    shadow.Append(Binding::Create(ONYX_INSTANCES_BINDING_POINT, buffer, vertex | fragment));
    shadow.Append(Binding::Create(ONYX_SAMPLERS_BINDING_POINT, sampler, fragment, ONYX_MAX_SAMPLERS, pbound));
    shadow.Append(Binding::Create(ONYX_TEXTURES_BINDING_POINT, sampledImage, fragment, ONYX_MAX_TEXTURES, pbound));
    shadow.Append(Binding::Create(ONYX_BOUNDS_BINDING_POINT, buffer, vertex));

    s_DescriptorData->Layouts[Dim3][RenderPass_Shadow] = Graph::DescriptorSetLayout_Create(shadow);

    shadow.Append(Binding::Create(ONYX_MATERIALS_BINDING_POINT, buffer, fragment));
    s_DescriptorData->Layouts[Dim2][RenderPass_Shadow] = Graph::DescriptorSetLayout_Create(shadow);

    // NOTE(Isma): Removed bind unused here
    const Binding rayMarch[] = {
        Binding::Create(ONYX_OCCLUSION_MAP_BINDING_POINT, storageImage, compute,
                        ONYX_MAX_RAY_MARCH_AND_OCCLUSION_MAP_SIZE, pbound),
        Binding::Create(ONYX_RAY_MARCH_MAP_BINDING_POINT, storageImage, compute,
                        ONYX_MAX_RAY_MARCH_AND_OCCLUSION_MAP_SIZE, pbound),
    };
    s_DescriptorData->StandaloneLayouts[StandalonePass_RayMarch] = Graph::DescriptorSetLayout_Create(rayMarch);

    constexpr Binding blend[] = {
        Binding::Create(ONYX_BLEND_TRANSPARENT_ATTACHMENTS_BINDING, combined, fragment, ONYX_MAX_ATTACHMENTS, pbound),
        Binding::Create(ONYX_BLEND_REVEALAGE_ATTACHMENTS_BINDING, combined, fragment, ONYX_MAX_ATTACHMENTS, pbound),
    };
    s_DescriptorData->StandaloneLayouts[StandalonePass_Blend] = Graph::DescriptorSetLayout_Create(blend);

    constexpr Binding postProcess[] = {
        Binding::Create(ONYX_POST_PROCESS_COLOR_ATTACHMENTS_BINDING, combined, fragment, ONYX_MAX_ATTACHMENTS, pbound),
        Binding::Create(ONYX_POST_PROCESS_OUTLINE_ATTACHMENTS_BINDING, combined, fragment, ONYX_MAX_ATTACHMENTS,
                        pbound),
        Binding::Create(ONYX_POST_PROCESS_STENCIL_ATTACHMENTS_BINDING, sampledImage, fragment, ONYX_MAX_ATTACHMENTS,
                        pbound),
    };
    s_DescriptorData->StandaloneLayouts[StandalonePass_PostProcess] = Graph::DescriptorSetLayout_Create(postProcess);

    constexpr Binding compositor[] = {
        Binding::Create(ONYX_COMPOSITOR_COLOR_ATTACHMENTS_BINDING, combined, fragment, ONYX_MAX_ATTACHMENTS, pbound),
    };
    s_DescriptorData->StandaloneLayouts[StandalonePass_Compositor] = Graph::DescriptorSetLayout_Create(compositor);

    if (Graph::IsValidationEnabled())
    {
        s_DescriptorData->Layouts.IterateMultiIndex([&](const u32 i, const u32 j) {
            Graph::DescriptorSetLayout_SetName(
                s_DescriptorData->Layouts[i][j],
                TKit::StackString::Format("onyx-descriptor-set-layout-{}D-{}", i + 2, ToString(RenderPass(j)))
                    .CString());
        });

        Graph::DescriptorSetLayout_SetName(s_DescriptorData->StandaloneLayouts[StandalonePass_RayMarch],
                                           "onyx-ray-march-descriptor-set-layout");
        Graph::DescriptorSetLayout_SetName(s_DescriptorData->StandaloneLayouts[StandalonePass_Blend],
                                           "onyx-blend-descriptor-set-layout");
        Graph::DescriptorSetLayout_SetName(s_DescriptorData->StandaloneLayouts[StandalonePass_PostProcess],
                                           "onyx-post-process-descriptor-set-layout");
        Graph::DescriptorSetLayout_SetName(s_DescriptorData->StandaloneLayouts[StandalonePass_Compositor],
                                           "onyx-compositor-descriptor-set-layout");
    }
}

void Initialize()
{
    TKIT_LOG_INFO("[ONYX][DESCRIPTORS] Initializing");
    s_DescriptorData.Construct();
    return createDescriptorData();
}
void Terminate()
{
    for (const Graph::DescriptorSetLayout layout : s_DescriptorData->Layouts)
        Graph::DescriptorSetLayout_Destroy(layout);

    for (const Graph::DescriptorSetLayout layout : s_DescriptorData->StandaloneLayouts)
        Graph::DescriptorSetLayout_Destroy(layout);
    s_DescriptorData.Destruct();
}

template <Dimension D> Graph::DescriptorSetLayout GetDescriptorLayout(const RenderPass pass)
{
    return s_DescriptorData->Layouts[D - 2][pass];
}
Graph::DescriptorSetLayout GetDescriptorLayout(const StandalonePass pass)
{
    return s_DescriptorData->StandaloneLayouts[pass];
}

template Graph::DescriptorSetLayout GetDescriptorLayout<D2>(RenderPass pass);
template Graph::DescriptorSetLayout GetDescriptorLayout<D3>(RenderPass pass);
} // namespace Onyx::Descriptors
