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

    // we do this so that render texture A, created before render texture B, may sample render texture B. this happens
    // in a loop, and A may sample texture B in the wrong layout. So we expose this method so that all textures can be
    // transitioned once
    void PrepareReadableImage(Graph::CommandBuffer cmd);

    void BeginRendering(Graph::CommandBuffer cmd);
    void EndRendering(Graph::CommandBuffer cmd);

    void MarkWriteImageInUse(const Graph::Tracker &tracker);
    void MarkReadImageInUse(const Graph::Tracker &tracker);
    const u32v2 &GetSize() const
    {
        return m_Size;
    }
    void Resize(const u32v2 &dims, const f32v2 &contentScale);
    void Resize(const u32v2 &dims)
    {
        Resize(dims, m_ContentScale);
    }

    void SetContentScale(const f32v2 &contentScale)
    {
        Resize(m_Size, contentScale);
    }
    const f32v2 &GetContentScale() const
    {
        return m_ContentScale;
    }
    // the size of the images in physical pixels: logical size x content scale
    u32v2 GetPixelSize() const
    {
        return ApplyScale(m_Size, m_ContentScale);
    }
    usz GetBytes() const
    {
        return Graph::Image_GetBytes(m_Images[m_Readable].Image);
    }

    // TODO(Isma): We should give caller a choice on how to allocate these
    /**
     * @brief Read back the image rendered by the last call to `Onyx::Render()`.
     *
     * The pixels are 8 bit RGBA, sRGB encoded (what a window would display), rows from top to bottom, and there are
     * `GetPixelSize()` of them. It waits for the device to finish that image, so it is slow: meant for tests,
     * screenshots and tools, not for every frame. Before the first render there is nothing to read and the array is
     * empty.
     */
    bool ReadPixels(u8 *data) const;

    operator u32() const
    {
        return m_Handle;
    }

  private:
    void createSecondaryImages();
    u32v2 getSize() const override
    {
        return m_Size;
    }
    f32v2 getContentScale() const override
    {
        return m_ContentScale;
    }

    void drainWork() const;

    Resource m_Handle;
    u32v2 m_Size;
    f32v2 m_ContentScale;

    u32 m_Writable = 0;
    u32 m_Readable = 0;
    u32 m_ImageCount;

    TKit::TierArray<FrontEndImage> m_Images{};
};
} // namespace Onyx
