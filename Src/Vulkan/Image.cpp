#include "DnmGLLite/Vulkan/Image.hpp"
#include "DnmGLLite/Vulkan/CommandBuffer.hpp"
#include <vma/vk_mem_alloc.h>
#include <ranges>

namespace DnmGLLite::Vulkan {
    static vk::ImageViewType GetVkImageViewType(ImageResourceType type) {
        switch (type) {
            case ImageResourceType::e1D: return vk::ImageViewType::e1D;
            case ImageResourceType::e2D: return vk::ImageViewType::e2D;
            case ImageResourceType::e2DArray: return vk::ImageViewType::e2DArray;
            case ImageResourceType::eCube: return vk::ImageViewType::eCube;
            case ImageResourceType::e3D: return vk::ImageViewType::e3D;
        }
    }

    static vk::ImageType GetVkImageType(ImageType type) {
        switch (type) {
            case ImageType::e1D: return vk::ImageType::e1D;
            case ImageType::e2D: return vk::ImageType::e2D;
            case ImageType::e3D: return vk::ImageType::e3D;
        }
    }

    static vk::ImageUsageFlags GetVkImageUsageFlags(ImageUsageFlags usage_flags) {
        vk::ImageUsageFlags vk_flags{};

        if (usage_flags.Has(ImageUsageBits::eColorAttachment)) {
            vk_flags |= vk::ImageUsageFlagBits::eColorAttachment;
        }

        if (usage_flags.Has(ImageUsageBits::eDepthStencilAttachment)) {
            vk_flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
        }

        if (usage_flags.Has(ImageUsageBits::eStorage)) {
            vk_flags |= vk::ImageUsageFlagBits::eStorage;
        }

        if (usage_flags.Has(ImageUsageBits::eSampled)) {
            vk_flags |= vk::ImageUsageFlagBits::eSampled;
        }

        if (usage_flags.Has(ImageUsageBits::eTransientAttachment)) {
            vk_flags |= vk::ImageUsageFlagBits::eTransientAttachment;
            return vk_flags;
        }
        vk_flags |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
        return vk_flags;
    }

    Image::Image(Vulkan::Context& ctx, const DnmGLLite::ImageDesc& desc)
    : DnmGLLite::Image(ctx, desc) {
        DnmGLLiteAssert(m_desc.mipmap_levels != 0, "mipmap level cannot be 0")
        DnmGLLiteAssert(m_desc.extent.x != 0 || m_desc.extent.y != 0 || m_desc.extent.z != 0, "extent values cannot be 0")
        if (m_desc.type == ImageType::e1D) {
            DnmGLLiteAssert((m_desc.extent.y == 1 && m_desc.extent.z == 1), 
                                "type if ImageType::e1D, x and z extent is must be 1 (1D arrays not supported)")
        }

        {
            switch (m_desc.format) {
                case Format::eD16Norm: 
                case Format::eD32Float: m_aspect = vk::ImageAspectFlagBits::eDepth; break;
                case Format::eD16NormS8UInt:
                case Format::eD24NormS8UInt:
                case Format::eD32NormS8UInt: m_aspect = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil; break;
                case Format::eS8UInt: m_aspect = vk::ImageAspectFlagBits::eStencil; break;
                default: m_aspect = vk::ImageAspectFlagBits::eColor; break;
            }
        }

        vk::ImageCreateFlags flags{};
        if (m_desc.type == ImageType::e3D) flags |= vk::ImageCreateFlagBits::e2DArrayCompatible;
        if (m_desc.type == ImageType::e2D && m_desc.extent.z >= 6) flags |= vk::ImageCreateFlagBits::eCubeCompatible;

        vk::ImageCreateInfo create_info{};
        create_info.setInitialLayout(vk::ImageLayout::ePreinitialized)
                    .setImageType(GetVkImageType(m_desc.type))
                    .setArrayLayers((m_desc.type == ImageType::e2D) ? m_desc.extent.z : 1u)
                    .setExtent(vk::Extent3D(m_desc.extent.x, m_desc.extent.y, (m_desc.type == ImageType::e3D) ? 1 : m_desc.extent.z))
                    .setFlags(flags)
                    .setSamples(static_cast<vk::SampleCountFlagBits>(desc.sample_count))
                    .setSharingMode(vk::SharingMode::eExclusive)
                    .setTiling(vk::ImageTiling::eOptimal)
                    .setUsage(GetVkImageUsageFlags(m_desc.usage_flags))
                    .setFormat(static_cast<vk::Format>(m_desc.format))
                    .setMipLevels(m_desc.mipmap_levels)
                    ;

        VmaAllocationCreateInfo alloc_create_info{};
        alloc_create_info.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
        alloc_create_info.priority = 1.f;

        VmaAllocationInfo alloc_info;
        auto result = (vk::Result)vmaCreateImage(
            VulkanContext->GetVmaAllocator(), 
            reinterpret_cast<VkImageCreateInfo*>(&create_info), 
            &alloc_create_info, 
            reinterpret_cast<VkImage*>(&m_image), 
            &m_allocation, 
            &alloc_info);

        if (result == vk::Result::eErrorOutOfDeviceMemory || result == vk::Result::eErrorOutOfHostMemory) {
            VulkanContext->Message("vmaCreateImage create buffer failed, out of memory", MessageType::eOutOfMemory);
        }
        else if (result != vk::Result::eSuccess) {
            VulkanContext->Message("vmaCreateImage create buffer failed, unknown", MessageType::eUnknown);
        }

        const auto* command_buffer = VulkanContext->GetCommandBufferIfRecording();
        if (command_buffer) {
            const TransferImageLayoutDesc transfer_layout_desc{
                .image = this,
                .new_image_layout = Image::GetIdealImageLayout(),
                .src_pipeline_stages = vk::PipelineStageFlagBits::eBottomOfPipe,
                .dst_pipeline_stages = vk::PipelineStageFlagBits::eTopOfPipe,
                .src_access = {},
                .dst_access = {},
            };
            command_buffer->TransferImageLayout({
                &transfer_layout_desc,
                1
            });
        }
        else {
            VulkanContext->DeferImageLayoutTransfer({
                m_image,
                m_aspect,
                Image::GetIdealImageLayout()
            });
            m_image_layout = Image::GetIdealImageLayout();
        }
    }

    Image::~Image() {
        const auto image = m_image;
        const auto image_views = std::move(m_image_views);
        auto* allocation = m_allocation;
        VulkanContext->DeleteObject(
            [image, allocation, image_views] (vk::Device device, VmaAllocator allocator) -> void {
            for (auto image_view : image_views | std::ranges::views::values)
                device.destroy(image_view);

            vmaDestroyImage(allocator, image, allocation);
        });
    }

    vk::ImageView Image::CreateGetImageView(const ImageSubresource& subresource) {
        auto [it, is_inserted] = m_image_views.try_emplace(subresource, VK_NULL_HANDLE);
        if (!is_inserted) { return it->second; }

        vk::ImageViewCreateInfo create_info{};
        create_info.setImage(m_image)
                    .setViewType(GetVkImageViewType(subresource.type))
                    .setFormat(static_cast<vk::Format>(m_desc.format))
                    .setSubresourceRange(
                    vk::ImageSubresourceRange{}
                        .setAspectMask(GetAspect())
                        .setBaseArrayLayer(subresource.base_layer)
                        .setBaseMipLevel(subresource.base_mipmap)
                        .setLayerCount(subresource.layer_count)
                        .setLevelCount(subresource.mipmap_level));

        const auto image_view  = VulkanContext->GetDevice().createImageView(create_info);
        it->second = image_view;
        return image_view;
    }
}