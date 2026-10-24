#pragma once

#include "onyx/render_target.hpp"
#include "onyx/handle.hpp"
#include "graph/execution.hpp"

namespace Onyx
{
struct FrontEndImage
{
    Graph::Image Image;
    Graph::Tracker Tracker{};
    Resource Texture;
};

struct RenderTextureSpecs
{
    u32 ImageCount = 3;
    f32 ContentScale = 1.f;
};

class RenderTexture final : public RenderTarget
{
  public:
    RenderTexture(const u32v2 &dimensions, const RenderTextureSpecs &specs);
    ~RenderTexture();

    void FindAvailableImages();

    void BeginRendering(Graph::CommandBuffer cmd);
    void EndRendering(Graph::CommandBuffer cmd);

    void MarkWriteImageInUse(const Graph::Tracker &tracker);
    void MarkReadImageInUse(const Graph::Tracker &tracker);
    const u32v2 &GetDimensions() const
    {
        return m_Dimensions;
    }
    void Resize(const u32v2 &dims, const f32v2 &contentScale);
    void Resize(const u32v2 &dims)
    {
        Resize(dims, m_ContentScale);
    }

    void SetContentScale(const f32v2 &contentScale)
    {
        Resize(m_Dimensions, contentScale);
    }
    const f32v2 &GetContentScale() const
    {
        return m_ContentScale;
    }

    operator u32() const
    {
        return m_Handle;
    }

  private:
    void createSecondaryImages();
    u32v2 getDimensions() const override
    {
        return m_Dimensions;
    }
    u32v2 getExtent() const override
    {
        return ComputeExtent(m_Dimensions, m_ContentScale);
    }
    f32v2 getContentScale() const override
    {
        return m_ContentScale;
    }

    void drainWork() const;

    Resource m_Handle;
    u32v2 m_Dimensions;
    f32v2 m_ContentScale;

    u32 m_Writable = 0;
    u32 m_Readable = 0;
    u32 m_ImageCount;

    TKit::TierArray<FrontEndImage> m_Images{};
};
} // namespace Onyx
