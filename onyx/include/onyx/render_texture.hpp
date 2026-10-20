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

class RenderTexture final : public RenderTarget
{
  public:
    RenderTexture(const u32v2 &dimensions, u32 imageCount = 3);
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
    void Resize(const u32v2 &dims);

    operator u32() const
    {
        return m_Handle;
    }

  private:
    void createSecondaryImages();
    u32v2 getExtent() const override
    {
        return m_Dimensions;
    }

    void drainWork() const;

    Resource m_Handle;
    u32v2 m_Dimensions;

    u32 m_Writable = 0;
    u32 m_Readable = 0;
    u32 m_ImageCount;

    TKit::TierArray<FrontEndImage> m_Images{};
};
} // namespace Onyx
