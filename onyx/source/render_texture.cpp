#include "pch.hpp"
#include "onyx/render_texture.hpp"
#include "resources.hpp"
#include "core.hpp"
#include "tkit/profiling/macros.hpp"

namespace Onyx
{
static Graph::Image createImage(const u32v2 &size, const f32v2 contentScale)
{
    const Graph::Format srgbFmt = GetAttachmentFormat(AttachmentKind_SrgbColor);
    const Graph::Format linearFmt = GetAttachmentFormat(AttachmentKind_LinearColor);

    const TKit::FixedArray<Graph::Format, 2> formats{srgbFmt, linearFmt};
    const TKit::FixedArray<Graph::ImageViewSpecs, 2> views{{.Format = srgbFmt}, {.Format = linearFmt}};

    Graph::ImageSpecs specs{};
    specs.Formats = formats;
    specs.ImageViews = views;

    return Graph::Image_Create(ApplyScale(size, contentScale), specs,
                               Graph::ImageFlag_ColorAttachment | Graph::ImageFlag_Sampled | Graph::ImageFlag_Source |
                                   Graph::ImageFlag_Destination);
}

static void nameImage(const Graph::Image img, const u32 idx)
{
    const TKit::StackString nimg = TKit::Format("onyx-render-texture-image-{}", idx);
    const TKit::StackString nview = TKit::Format("onyx-render-texture-image-view-{}", idx);
    Graph::Image_SetName(img, nimg.CString());
    Graph::Image_SetViewNames(img, nview.CString());
}

RenderTexture::RenderTexture(const u32v2 &size, const RenderTextureSpecs &specs)
    : m_Size(size), m_ContentScale(specs.ContentScale), m_ImageCount(specs.ImageCount)
{
    TKIT_ASSERT(specs.ImageCount >= 2, "[ONYX][RENDER-TEXTURE] At least two images are required for a render texture");
    FrontEndImage &img = m_Images.Append();

    img.Image = createImage(size, specs.ContentScale);
    img.Texture = Texture_CreateMainRenderTexture(Graph::Image_GetView(img.Image, 0));

    if (Graph::IsValidationEnabled())
        nameImage(img.Image, m_Images.GetSize() - 1);

    m_Handle = img.Texture;
    createSecondaryImages();
}
RenderTexture::~RenderTexture()
{
    drainWork();
    for (FrontEndImage &img : m_Images)
    {
        // may be the case that resources already freed everything on global tear-down
        if (IsResourceValid(img.Texture))
            Texture_Destroy(img.Texture);
        Graph::Image_Destroy(img.Image);
    }
}

void RenderTexture::Resize(const u32v2 &dims, const f32v2 &contentScale)
{
    drainWork();

    FrontEndImage &main = m_Images.GetFront();
    const FrontEndImage mostUpToDate = m_Images[m_Writable];

    const auto destroyImage = [&](const FrontEndImage &img) {
        Texture_Destroy(img.Texture);
        Graph::Image_Destroy(img.Image);
    };
    for (u32 i = 1; i < m_Images.GetSize(); ++i)
        if (i != m_Writable)
            destroyImage(m_Images[i]);

    m_Images.Resize(1);

    const Graph::Image newImage = createImage(dims, contentScale);

    Graph::CommandPool_ImmediateSubmission(GetTransientGraphicsCommandPool(), [&](const Graph::CommandBuffer cmd) {
        TKit::FixedArray<Graph::ImageMemoryBarrierInfo, 2> infos{};

        infos[0].Image = mostUpToDate.Image;
        infos[0].NewLayout = Graph::ImageLayout_TransferSrc;
        infos[0].DstAccess = Graph::AccessFlag_TransferRead;
        infos[0].DstStages = Graph::PipelineStageFlag_Transfer;

        infos[1].Image = newImage;
        infos[1].NewLayout = Graph::ImageLayout_TransferDst;
        infos[1].DstAccess = Graph::AccessFlag_TransferWrite;
        infos[1].DstStages = Graph::PipelineStageFlag_Transfer;

        Graph::Command_ImageBarrier(cmd, infos);

        Graph::Command_BlitImage(cmd, mostUpToDate.Image, newImage);

        Graph::ImageMemoryBarrierInfo binfo{};
        binfo.Image = newImage;
        binfo.NewLayout = Graph::ImageLayout_ShaderReadOnly;
        binfo.SrcAccess = Graph::AccessFlag_TransferWrite;
        binfo.SrcStages = Graph::PipelineStageFlag_Transfer;

        Graph::Command_ImageBarrier(cmd, binfo);
    });

    if (m_Writable != 0)
        destroyImage(mostUpToDate);

    m_Writable = 0;
    m_Readable = 0;

    Graph::Image_Destroy(main.Image);
    main.Image = newImage;

    if (Graph::IsValidationEnabled())
        nameImage(newImage, 0);

    Texture_UpdateRenderTexture(main.Texture, Graph::Image_GetView(main.Image, 0));
    m_Size = dims;
    m_ContentScale = contentScale;
    updateRenderViews();
    createSecondaryImages();
}

void RenderTexture::FindAvailableImages()
{
    m_Readable = m_Writable; // the one we just wrote to (most updated one)
    Texture_UpdateHandleOffset(m_Handle, m_Images[m_Readable].Texture);

    u32 pending = TKIT_U32_MAX;
    for (u32 i = 0; i < m_Images.GetSize(); ++i)
    {
        if (i == m_Readable)
            continue;
        if (!m_Images[i].Tracker.InUse())
        {
            m_Writable = i;
            return;
        }
        else if (pending == TKIT_U32_MAX || m_Images[pending].Tracker.InFlightValue > m_Images[i].Tracker.InFlightValue)
            pending = i;
    }

    TKIT_ASSERT(pending != TKIT_U32_MAX, "[ONYX][RENDER-TEXTURE] Failed to find an available image");
    TKIT_BEGIN_INFO_CLOCK();
    Graph::Queue_WaitForTracker(m_Images[pending].Tracker);
    TKIT_END_INFO_CLOCK(Milliseconds, "[ONYX][RENDER-TEXTURE] Waited {:.2f} ms for a new image");
    m_Writable = pending;
}

void RenderTexture::PrepareReadableImage(const Graph::CommandBuffer cmd)
{
    const FrontEndImage &read = m_Images[m_Readable];
    // the readable image is only in attachment layout if it was rendered to last frame. after a resize (or on the
    // first frame) it is already shader read only, and the transition would be a no-op with a wrong src access
    if (Graph::Image_GetLayout(read.Image) != Graph::ImageLayout_ColorAttachment)
        return;

    Graph::ImageMemoryBarrierInfo rb{};
    rb.Image = read.Image;
    rb.NewLayout = Graph::ImageLayout_ShaderReadOnly;
    rb.SrcStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    rb.SrcAccess = Graph::AccessFlag_ColorAttachmentWrite;
    rb.DstStages = Graph::PipelineStageFlag_FragmentShader;
    rb.DstAccess = Graph::AccessFlag_ShaderRead;

    Graph::Command_ImageBarrier(cmd, rb);
}

void RenderTexture::BeginRendering(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderTexture::BeginRendering");
    TKIT_ASSERT(m_Writable != m_Readable,
                "[ONYX][RENDER-TEXTURE] Cannot have the same image as a readable and writable texture!");

    const FrontEndImage &write = m_Images[m_Writable];

    Graph::ImageMemoryBarrierInfo wb{};
    wb.Image = write.Image;
    wb.NewLayout = Graph::ImageLayout_ColorAttachment;
    wb.SrcStages = Graph::PipelineStageFlag_FragmentShader;
    wb.DstStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    wb.DstAccess = Graph::AccessFlag_ColorAttachmentWrite;

    Graph::Command_ImageBarrier(cmd, wb);
    Graph::AttachmentInfo target{};
    target.View = Graph::Image_GetView(write.Image, 1);
    target.Layout = Graph::ImageLayout_ColorAttachment;
    target.LoadOp = Graph::LoadOp_Clear;
    target.StoreOp = Graph::StoreOp_Store;
    target.ClearValue.Color = ClearColor.rgba;

    Graph::RenderPassInfo info{};
    info.RenderArea = {{0, 0}, ApplyScale(m_Size, m_ContentScale)};
    info.ColorAttachments = target;
    Graph::Command_BeginRenderPass(cmd, info);
}
void RenderTexture::EndRendering(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderTexture::EndRendering");

    // no sync. it is synced in begin, when this becomes a readable texture
    Graph::Command_EndRenderPass(cmd);
}

void RenderTexture::MarkWriteImageInUse(const Graph::Tracker &tracker)
{
    m_Images[m_Writable].Tracker = tracker;
}
void RenderTexture::MarkReadImageInUse(const Graph::Tracker &tracker)
{
    m_Images[m_Readable].Tracker = tracker;
}

void RenderTexture::createSecondaryImages()
{
    TKIT_ASSERT(m_Images.GetSize() == 1,
                "[ONYX][RENDER-TEXTURE] When creating secondary images, the image array size must be one, but is {}",
                m_Images.GetSize());

    for (u32 i = 1; i < m_ImageCount; ++i)
    {
        FrontEndImage &img = m_Images.Append();
        img.Image = createImage(m_Size, m_ContentScale);
        img.Texture = Texture_CreateSecondaryRenderTexture(Graph::Image_GetView(img.Image, 0));
        if (Graph::IsValidationEnabled())
            nameImage(img.Image, i);
    }
}

void RenderTexture::drainWork() const
{
    for (const FrontEndImage &img : m_Images)
        Graph::Queue_WaitForTracker(img.Tracker);
    // Graph::DeviceWaitIdle();
}
} // namespace Onyx
