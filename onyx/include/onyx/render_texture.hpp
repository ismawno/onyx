#pragma once

#include "onyx/render_target.hpp"
#include "onyx/handle.hpp"

namespace Onyx
{
struct FrontEndImage;

class RenderTexture final : public RenderTarget
{
  public:
    RenderTexture(const u32v2 &dimensions);
    ~RenderTexture();

    void FindAvailableImages();

    void BeginRendering(Onyx_CommandBuffer commandBuffer);
    void EndRendering(Onyx_CommandBuffer commandBuffer);

    void MarkCurrentImageInUse(const Execution::Tracker &tracker);
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
    u32v2 getExtent() const override
    {
        return m_Dimensions;
    }

    Resource m_Handle;
    u32v2 m_Dimensions;

    u32 m_Writable = 0;
    u32 m_Readable = 0;

    TKit::TierArray<FrontEndImage *> m_Images{};
};
} // namespace Onyx
