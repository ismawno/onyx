#include "pch.hpp"
#include "onyx/render_texture.hpp"
#include "core.hpp"
#include "execution.hpp"
#include "resources.hpp"
#include "attachment.hpp"
#include "platform.hpp"
#include "tkit/profiling/macros.hpp"

namespace Onyx
{
struct FrontEndImage
{
    VKit::DeviceImage Image;
    Execution::Tracker Tracker{};
    Resource Texture;
};

static VKit::DeviceImage createImage(const u32v2 &dimensions)
{
    const auto &device = GetDevice();
    const VmaAllocator alloc = GetVulkanAllocator();

    const VkFormat format = GetAttachmentFormat(Attachment_Intermediate);
    const VkFormat sformat = Platform::GetSurfaceFormat().format;
    const TKit::FixedArray<VkFormat, 2> formats{format, sformat};

    return ONYX_CHECK_VKIT_RESULT(
        VKit::DeviceImage::Builder(device, alloc, VkExtent2D{.width = dimensions[0], .height = dimensions[1]}, formats,
                                   VKit::DeviceImageFlag_ColorAttachment | VKit::DeviceImageFlag_Sampled)
            .AddImageView(format)
            .AddImageView(sformat)
            .Build());
}

static void nameImage(VKit::DeviceImage &img, const u32 idx)
{
    const TKit::StackString nimg = TKit::Format("onyx-render-texture-image-{}", idx);
    const TKit::StackString nview = TKit::Format("onyx-render-texture-image-view-{}", idx);
    ONYX_CHECK_VKIT_RESULT(img.SetName(nimg.CString()));
    ONYX_CHECK_VKIT_RESULT(img.SetViewNames(nview.CString()));
}

RenderTexture::RenderTexture(const u32v2 &dimensions) : m_Dimensions(dimensions)
{
    TKit::TierAllocator *tier = GetTier();
    FrontEndImage *img = m_Images.Append(tier->Create<FrontEndImage>());

    img->Image = createImage(dimensions);
    img->Texture = Resources::CreateMainRenderTexture(img->Image.GetView());

    if (IsDebugUtilsEnabled())
        nameImage(img->Image, m_Images.GetSize() - 1);

    m_Handle = img->Texture;
}
RenderTexture::~RenderTexture()
{
    for (FrontEndImage *img : m_Images)
    {
        // may be the case that resources already freed everything on global tear-down
        if (Resources::IsResourceValid(img->Texture))
            Resources::DestroyTexture(img->Texture);
        img->Image.Destroy();
    }
}

void RenderTexture::SetDimensions(const u32v2 &dims)
{
    m_Dimensions = dims;
    TKit::StackArray<VkSemaphore> semaphores{};
    semaphores.Reserve(m_Images.GetSize());

    TKit::StackArray<u64> values{};
    values.Reserve(m_Images.GetSize());

    for (const FrontEndImage *img : m_Images)
        if (img->Tracker.InFlight())
        {
            semaphores.Append(img->Tracker.Queue->GetTimelineSempahore());
            values.Append(img->Tracker.InFlightValue);
        }

    if (!semaphores.IsEmpty())
    {
        const auto table = GetDeviceTable();
        VkSemaphoreWaitInfoKHR waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
        waitInfo.semaphoreCount = semaphores.GetSize();
        waitInfo.pSemaphores = semaphores.GetData();
        waitInfo.pValues = values.GetData();

        const auto &device = GetDevice();
        ONYX_CHECK_VKIT_RESULT(table->WaitSemaphoresKHR(device, &waitInfo, TKIT_U64_MAX));
    }

    FrontEndImage *main = m_Images.GetFront();
    TKit::TierAllocator *tier = GetTier();
    for (u32 i = 1; i < m_Images.GetSize(); ++i)
    {
        Resources::DestroyTexture(m_Images[i]->Texture);
        m_Images[i]->Image.Destroy();
        tier->Destroy(m_Images[i]);
    }
    m_Images.Resize(1);

    m_Writable = 0;
    m_Readable = 0;

    main->Image.Destroy();
    main->Image = createImage(dims);
    if (IsDebugUtilsEnabled())
        nameImage(main->Image, 0);

    Resources::UpdateRenderTexture(main->Texture, main->Image.GetView());
}

void RenderTexture::FindAvailableImages()
{
    m_Readable = m_Writable; // the one we just wrote to (most updated one)
    Resources::UpdateTextureHandleOffset(m_Handle, m_Images[m_Readable]->Texture);

    for (u32 i = 0; i < m_Images.GetSize(); ++i)
        if (i != m_Readable && !m_Images[i]->Tracker.InUse())
        {
            m_Writable = i;
            return;
        }

    m_Writable = m_Images.GetSize();
    TKIT_LOG_DEBUG("[ONYX][RENDER-TEXTURE] Failed to find an available writable image. Increasing image pool to {} for "
                   "this particular render texture",
                   m_Writable + 1);

    TKit::TierAllocator *tier = GetTier();
    FrontEndImage *img = m_Images.Append(tier->Create<FrontEndImage>());

    img->Image = createImage(m_Dimensions);
    img->Texture = Resources::CreateSecondaryRenderTexture(img->Image.GetView());
    if (IsDebugUtilsEnabled())
        nameImage(img->Image, m_Writable);
}

void RenderTexture::BeginRendering(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderTexture::BeginRendering");
    TKIT_ASSERT(m_Writable != m_Readable,
                "[ONYX][RENDER-TEXTURE] Cannot have the same image as a readable and writable texture!");

    FrontEndImage *read = m_Images[m_Readable];
    FrontEndImage *write = m_Images[m_Writable];

    read->Image.TransitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 {
                                     .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
                                     .DstAccess = VK_ACCESS_2_SHADER_READ_BIT_KHR,

                                     .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                     .DstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                 });

    write->Image.TransitionLayout(cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  {.DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,

                                   .SrcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
                                   .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR});

    VkRenderingAttachmentInfoKHR target{};
    target.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    target.imageView = write->Image.GetView(1);
    target.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    target.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    target.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    target.clearValue.color = {{ClearColor.rgba[0], ClearColor.rgba[1], ClearColor.rgba[2], ClearColor.rgba[3]}};

    const VkExtent2D extent = {m_Dimensions[0], m_Dimensions[1]};
    VkRenderingInfoKHR renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderInfo.renderArea = {{0, 0}, extent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &target;

    const auto table = GetDeviceTable();
    table->CmdBeginRenderingKHR(cmd, &renderInfo);
}
void RenderTexture::EndRendering(const VkCommandBuffer cmd)
{
    TKIT_PROFILE_NSCOPE("Onyx::RenderTexture::EndRendering");

    // no sync. it is synced in begin, when this becomes a readable texture
    const auto table = GetDeviceTable();
    table->CmdEndRenderingKHR(cmd);
}

void RenderTexture::MarkCurrentImageInUse(const Execution::Tracker &tracker)
{
    m_Images[m_Writable]->Tracker = tracker;
}

} // namespace Onyx
