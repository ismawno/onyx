#include "pch.hpp"
#include "onyx/render_texture.hpp"
#include "resources.hpp"
#include "core.hpp"
#include "tkit/profiling/macros.hpp"

namespace Onyx
{
static Graph::Image createImage(const u32v2 &dimensions)
{

    const Graph::Format format = GetAttachmentFormat(Attachment_Intermediate);
    const TKit::FixedArray<Graph::Format, 2> formats{format, SurfaceFormat};
    const TKit::FixedArray<Graph::ImageViewSpecs, 2> views{{.Format = format}, {.Format = SurfaceFormat}};

    Graph::ImageSpecs specs{};
    specs.Formats = formats;
    specs.ImageViews = views;

    return Graph::Image_Create(dimensions, specs,
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

RenderTexture::RenderTexture(const u32v2 &dimensions) : m_Dimensions(dimensions)
{
    FrontEndImage &img = m_Images.Append();

    img.Image = createImage(dimensions);
    img.Texture = Texture_CreateMainRenderTexture(Graph::Image_GetView(img.Image, 0));

    if (Graph::IsValidationEnabled())
        nameImage(img.Image, m_Images.GetSize() - 1);

    m_Handle = img.Texture;
}
RenderTexture::~RenderTexture()
{
    drainWork();
    for (FrontEndImage &img : m_Images)
    {
        // may be the case that resources already freed everything on global tear-down
        if (IsResourceValid(img.Texture))
            Texture_Destroy(img.Texture);
        Image_Destroy(img.Image);
    }
}

void RenderTexture::Resize(const u32v2 &dims)
{
    drainWork();

    FrontEndImage &main = m_Images.GetFront();
    const FrontEndImage mostUpToDate = m_Images[m_Writable];

    const auto destroyImage = [&](const FrontEndImage &img) {
        Texture_Destroy(img.Texture);
        Image_Destroy(img.Image);
    };
    for (u32 i = 1; i < m_Images.GetSize(); ++i)
        if (i != m_Writable)
            destroyImage(m_Images[i]);

    m_Images.Resize(1);

    m_Writable = 0;
    m_Readable = 0;

    const Graph::Image newImage = createImage(dims);

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

    Image_Destroy(main.Image);
    main.Image = newImage;

    if (Graph::IsValidationEnabled())
        nameImage(newImage, 0);

    Texture_UpdateRenderTexture(main.Texture, Graph::Image_GetView(main.Image, 0));
    m_Dimensions = dims;
    updateRenderViews();
}

void RenderTexture::FindAvailableImages()
{
    m_Readable = m_Writable; // the one we just wrote to (most updated one)
    Texture_UpdateHandleOffset(m_Handle, m_Images[m_Readable].Texture);

    for (u32 i = 0; i < m_Images.GetSize(); ++i)
        if (i != m_Readable && !m_Images[i].Tracker.InUse())
        {
            m_Writable = i;
            return;
        }

    m_Writable = m_Images.GetSize();
    TKIT_LOG_DEBUG("[ONYX][RENDER-TEXTURE] Failed to find an available writable image. Increasing image pool to {} for "
                   "this particular render texture",
                   m_Writable + 1);

    FrontEndImage &img = m_Images.Append();

    img.Image = createImage(m_Dimensions);
    img.Texture = Texture_CreateSecondaryRenderTexture(Graph::Image_GetView(img.Image, 0));
    if (Graph::IsValidationEnabled())
        nameImage(img.Image, m_Writable);
}

void RenderTexture::BeginRendering(const Graph::CommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderTexture::BeginRendering");
    TKIT_ASSERT(m_Writable != m_Readable,
                "[ONYX][RENDER-TEXTURE] Cannot have the same image as a readable and writable texture!");

    const FrontEndImage &read = m_Images[m_Readable];
    const FrontEndImage &write = m_Images[m_Writable];

    Graph::ImageMemoryBarrierInfo barriers[2]{};

    barriers[0].Image = read.Image;
    barriers[0].NewLayout = Graph::ImageLayout_ShaderReadOnly;
    barriers[0].SrcStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    barriers[0].SrcAccess = Graph::AccessFlag_ColorAttachmentWrite;
    barriers[0].DstStages = Graph::PipelineStageFlag_FragmentShader;
    barriers[0].DstAccess = Graph::AccessFlag_ShaderRead;

    barriers[1].Image = write.Image;
    barriers[1].NewLayout = Graph::ImageLayout_ColorAttachment;
    barriers[1].SrcStages = Graph::PipelineStageFlag_FragmentShader;
    barriers[1].DstStages = Graph::PipelineStageFlag_ColorAttachmentOutput;
    barriers[1].DstAccess = Graph::AccessFlag_ColorAttachmentWrite;

    Graph::Command_ImageBarrier(cmd, barriers);

    Graph::AttachmentInfo target{};
    target.View = Graph::Image_GetView(write.Image, 1);
    target.Layout = Graph::ImageLayout_ColorAttachment;
    target.LoadOp = Graph::LoadOp_Clear;
    target.StoreOp = Graph::StoreOp_Store;
    target.ClearValue.Color = ClearColor.rgba;

    Graph::RenderPassInfo info{};
    info.RenderArea = {{0, 0}, m_Dimensions};
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

void RenderTexture::drainWork() const
{
    // NOTE(Isma, 06/07/26): This sync step is just not enough and right now, im not sure why as the tracker waits
    // should ensure all work with the images is done. there is something here im definitely missing, so ill just stick
    // a device wait idle here. resize operations are supposed to be expensive and not frequent anyways
    //
    // NOTE(Isma, 09/07/26): Turns out i think i know what it is. i was not assigning a tracker to readable images, so
    // gpu would read them while resizing. this is fixed now, but if errors still happen, revert do device wait idle
    //
    for (const FrontEndImage &img : m_Images)
        Graph::Queue_WaitForTracker(img.Tracker);
    // Graph::DeviceWaitIdle();
}
} // namespace Onyx
