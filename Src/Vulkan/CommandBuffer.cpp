#include "DnmGLLite/Vulkan/CommandBuffer.hpp"
#include "DnmGLLite/Vulkan/Pipeline.hpp"
#include "DnmGLLite/Vulkan/Buffer.hpp"
#include "DnmGLLite/Vulkan/Image.hpp"

namespace DnmGLLite::Vulkan {
    CommandBuffer::CommandBuffer(Vulkan::Context& context)
        : DnmGLLite::CommandBuffer(context) {
        vk::CommandBufferAllocateInfo alloc_descs;
        alloc_descs.setCommandBufferCount(1)
                    .setCommandPool(context.GetCommandPool())
                    .setLevel(vk::CommandBufferLevel::ePrimary);
    
        command_buffer = context.GetDevice().allocateCommandBuffers(alloc_descs)[0];
    }
    
    void CommandBuffer::BindPipeline(const DnmGLLite::ComputePipeline* pipeline) {
        const auto* typed_pipeline = static_cast<const Vulkan::ComputePipeline *>(pipeline);

        ProcressDeferTranslateImageLayout();

        command_buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eCompute, 
                        typed_pipeline->GetPipelineLayout(),
                        0,
                        typed_pipeline->GetDstSets(),
                        {});

        command_buffer.bindPipeline(
            vk::PipelineBindPoint::eCompute, 
            typed_pipeline->GetPipeline()
        );
    }

    void CommandBuffer::CopyImageToBuffer(const DnmGLLite::ImageToBufferCopyDesc& desc) {
        ResourceBarrier({
                {},
                ResourceAccessBit::eWrite,
                vk::PipelineStageFlagBits::eTransfer
            },
            {
                {},
                ResourceAccessBit::eRead,
                vk::PipelineStageFlagBits::eTransfer
            }
        );

        auto* typed_src_image = static_cast<Vulkan::Image *>(desc.src_image);
        const auto* typed_dst_buffer = static_cast<const Vulkan::Buffer *>(desc.dst_buffer);
        
        if (typed_src_image->GetImageLayout() != vk::ImageLayout::eTransferSrcOptimal) {
            const TransferImageLayoutDesc transfer_image_layout {
                .image = typed_src_image,
                .new_image_layout = vk::ImageLayout::eTransferSrcOptimal,
                .src_pipeline_stages = vk::PipelineStageFlagBits::eTopOfPipe,
                .dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer,
                .src_access = {},
                .dst_access = vk::AccessFlagBits::eTransferRead,
            };

            TransferImageLayout({&transfer_image_layout, 1});
            AddImageForDeferTranslateLayout(typed_src_image);
        }

        const vk::BufferImageCopy buffer_image_copy {
            desc.buffer_offset,
            desc.buffer_row_lenght,
            desc.buffer_image_height,
            vk::ImageSubresourceLayers(
                typed_src_image->GetAspect(),
                desc.image_subresource.base_mipmap,
                desc.image_subresource.base_layer,
                desc.image_subresource.layer_count
            ),
            vk::Offset3D(desc.image_offset.x, desc.image_offset.y, desc.image_offset.z),
            vk::Extent3D(desc.image_extent.x, desc.image_extent.y, desc.image_extent.z)
        };

        command_buffer.copyImageToBuffer(
            typed_src_image->GetImage(), 
            typed_src_image->GetImageLayout(),
            typed_dst_buffer->GetBuffer(),
            buffer_image_copy
        );
    }

    void CommandBuffer::CopyImageToImage(const DnmGLLite::ImageToImageCopyDesc& desc) {
        ResourceBarrier({},
            {
                {},
                ResourceAccessBit::eRead | ResourceAccessBit::eWrite,
                vk::PipelineStageFlagBits::eTransfer
            }
        );

        auto* typed_src_image = static_cast<Vulkan::Image *>(desc.src_image);
        auto* typed_dst_image = static_cast<Vulkan::Image *>(desc.dst_image);
        
        // image layout translation
        {
            std::vector<TransferImageLayoutDesc> image_layout_translate{};
            image_layout_translate.reserve(2);
    
            if (typed_src_image->GetImageLayout() != vk::ImageLayout::eTransferSrcOptimal) {
                image_layout_translate.emplace_back(
                    typed_src_image,
                    vk::ImageLayout::eTransferSrcOptimal,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits{},
                    vk::AccessFlagBits::eTransferRead
                );
            }
            if (typed_dst_image->GetImageLayout() != vk::ImageLayout::eTransferDstOptimal) {
                image_layout_translate.emplace_back(
                    typed_dst_image,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::AccessFlagBits{},
                    vk::AccessFlagBits::eTransferWrite
                );
            }
            TransferImageLayout(image_layout_translate);

            AddImageForDeferTranslateLayout(typed_src_image);
            AddImageForDeferTranslateLayout(typed_dst_image);
        }
        
        const vk::ImageCopy image_copy(
            vk::ImageSubresourceLayers(
                typed_src_image->GetAspect(),
                desc.src_image_subresource.base_mipmap,
                desc.src_image_subresource.base_layer,
                desc.src_image_subresource.layer_count
            ),
            vk::Offset3D(desc.src_offset.x, desc.src_offset.y, desc.src_offset.z),
            vk::ImageSubresourceLayers(
                typed_src_image->GetAspect(),
                desc.dst_image_subresource.base_mipmap,
                desc.dst_image_subresource.base_layer,
                desc.dst_image_subresource.layer_count
            ),
            vk::Offset3D(desc.dst_offset.x, desc.dst_offset.y, desc.dst_offset.z),
            vk::Extent3D(desc.extent.x, desc.extent.y, desc.extent.z)
        );
        
        command_buffer.copyImage(
            typed_src_image->GetImage(),
            vk::ImageLayout::eTransferSrcOptimal,
            typed_dst_image->GetImage(),
            vk::ImageLayout::eTransferDstOptimal,
            image_copy
        );
    }

    void CommandBuffer::CopyBufferToBuffer(const DnmGLLite::BufferToBufferCopyDesc &desc) {
        ResourceBarrier({
                {},
                ResourceAccessBit::eRead | ResourceAccessBit::eWrite,
                vk::PipelineStageFlagBits::eTransfer
            },
            {}
        );

        const auto *typed_src_buffer = static_cast<const Vulkan::Buffer *>(desc.src_buffer);
        const auto *typed_dst_buffer = static_cast<const Vulkan::Buffer *>(desc.dst_buffer);

        const vk::BufferCopy buffer_copy {
            desc.src_offset,
            desc.dst_offset,
            desc.copy_size
        };

        command_buffer.copyBuffer(
            typed_src_buffer->GetBuffer(), 
            typed_dst_buffer->GetBuffer(), 
            buffer_copy);
    }

    void CommandBuffer::CopyBufferToImage(const DnmGLLite::BufferToImageCopyDesc& desc) {
        ResourceBarrier(
            {
                {},
                ResourceAccessBit::eRead,
                vk::PipelineStageFlagBits::eTransfer
            },
            {
                {},
                ResourceAccessBit::eWrite,
                vk::PipelineStageFlagBits::eTransfer
            }
        );

        const auto* typed_src_buffer = static_cast<const Vulkan::Buffer*>(desc.src_buffer);
        auto* typed_dst_image = static_cast<Vulkan::Image*>(desc.dst_image);

        if (typed_dst_image->GetImageLayout() != vk::ImageLayout::eTransferDstOptimal) {
            const TransferImageLayoutDesc transfer_image_layout {
                .image = typed_dst_image,
                .new_image_layout = vk::ImageLayout::eTransferDstOptimal,
                .src_pipeline_stages = vk::PipelineStageFlagBits::eTopOfPipe,
                .dst_pipeline_stages = vk::PipelineStageFlagBits::eTransfer,
                .src_access = {},
                .dst_access = vk::AccessFlagBits::eTransferWrite,
            };

            TransferImageLayout({&transfer_image_layout, 1});

            AddImageForDeferTranslateLayout(typed_dst_image);
        }

        const vk::BufferImageCopy buffer_image_copy {
            desc.buffer_offset,
            desc.buffer_row_lenght,
            desc.buffer_image_height,
            vk::ImageSubresourceLayers(
                typed_dst_image->GetAspect(),
                desc.image_subresource.base_mipmap,
                desc.image_subresource.base_layer,
                desc.image_subresource.layer_count
            ),
            vk::Offset3D(desc.image_offset.x, desc.image_offset.y, desc.image_offset.z),
            vk::Extent3D(desc.image_extent.x, desc.image_extent.y, desc.image_extent.z)
        };

        command_buffer.copyBufferToImage(
            typed_src_buffer->GetBuffer(), 
            typed_dst_image->GetImage(), 
            vk::ImageLayout::eTransferDstOptimal, 
            buffer_image_copy);
    }

    void CommandBuffer::PushConstant(
        const DnmGLLite::GraphicsPipeline* pipeline, 
        DnmGLLite::ShaderStageFlags pipeline_stage, 
        uint32_t offset, 
        uint32_t size, 
        const void *ptr) {
        const auto* typed_pipeline = static_cast<const GraphicsPipeline*>(pipeline);
        command_buffer.pushConstants(
            typed_pipeline->GetPipelineLayout(), 
            static_cast<vk::ShaderStageFlags>((uint32_t)pipeline_stage),
            offset, 
            size, 
            ptr);
    }

    void CommandBuffer::PushConstant(
        const DnmGLLite::ComputePipeline* pipeline, 
        DnmGLLite::ShaderStageFlags pipeline_stage, 
        uint32_t offset, 
        uint32_t size, 
        const void *ptr) {
        const auto* typed_pipeline = static_cast<const Vulkan::ComputePipeline*>(pipeline);
        command_buffer.pushConstants(
            typed_pipeline->GetPipelineLayout(), 
            static_cast<vk::ShaderStageFlags>((uint32_t)pipeline_stage), 
            offset, 
            size, 
            ptr);
    }

    void CommandBuffer::TransferImageLayout(
        std::span<const TransferImageLayoutDesc> descs) const {
        std::vector<TransferImageLayoutNativeDesc> barriers{};
        barriers.reserve(descs.size());

        for (const auto& desc : descs) {
            if (desc.new_image_layout == desc.image->GetImageLayout()) {
                continue;
            }
            barriers.emplace_back(
                desc.image->GetImage(),
                desc.image->GetAspect(),
                desc.image->GetImageLayout(),
                desc.new_image_layout,
                desc.src_pipeline_stages,
                desc.dst_pipeline_stages,
                desc.src_access,
                desc.dst_access
            );

            desc.image->m_image_layout = desc.new_image_layout;
        }

        TransferImageLayout(barriers);
    }

    void CommandBuffer::TransferImageLayoutDefaultVk(
        std::span<const TransferImageLayoutNativeDesc> descs) const {
        for (const auto& desc : descs) {
            const vk::ImageMemoryBarrier barrier {
                vk::ImageMemoryBarrier(
                desc.src_access,
                desc.dst_access,
                desc.old_image_layout,
                desc.new_image_layout,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                desc.image,
                vk::ImageSubresourceRange( // sub resource
                        desc.image_aspect, // aspect
                        0, // base mipmap
                        VK_REMAINING_MIP_LEVELS, // mipmap count
                        0, // base layer
                        VK_REMAINING_ARRAY_LAYERS // layer count
                    ))
            };

            command_buffer.pipelineBarrier(
                desc.src_pipeline_stages,
                desc.dst_pipeline_stages, 
                {}, 
                {}, 
                {}, 
                barrier);
        }
    }
    
    void CommandBuffer::TransferImageLayoutSync2(
        std::span<const TransferImageLayoutNativeDesc> descs) const {
        std::vector<vk::ImageMemoryBarrier2> barriers;
        barriers.reserve(descs.size());

        for (const auto& desc : descs) {
            barriers.emplace_back(
                static_cast<vk::PipelineStageFlags2>((uint32_t)desc.src_pipeline_stages),
                static_cast<vk::AccessFlags2>((uint32_t)desc.src_access),
                static_cast<vk::PipelineStageFlags2>((uint32_t)desc.dst_pipeline_stages),
                static_cast<vk::AccessFlags2>((uint32_t)desc.dst_access),
                desc.old_image_layout,
                desc.new_image_layout,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                desc.image,
                vk::ImageSubresourceRange( // sub resource
                        desc.image_aspect, // aspect
                        0, // base mipmap
                        VK_REMAINING_MIP_LEVELS, // mipmap count
                        0, // base layer
                        VK_REMAINING_ARRAY_LAYERS // layer count
                    )
            );
        }

        const vk::DependencyInfo dependency_descs {
            {},
            {},
            {},
            {},
            {},
            static_cast<uint32_t>(barriers.size()),
            barriers.data()
        };

        command_buffer.pipelineBarrier2KHR(
            dependency_descs, VulkanContext->GetDispatcher());
    }

    void CommandBuffer::GenerateMipmaps(DnmGLLite::Image* image) {
        auto& image_desc = image->GetDesc();
        auto* typed_image = static_cast<Vulkan::Image*>(image);

        const auto& barrier = [
            cmd_buffer = command_buffer, 
            vk_image = typed_image->GetImage(),
            vk_aspect = typed_image->GetAspect()
        ] (
            vk::AccessFlags src_access,
            vk::AccessFlags dst_access,
            vk::PipelineStageFlags src_pipeline_stage,
            vk::PipelineStageFlags dst_pipeline_stage,
            vk::ImageLayout old_image_layout,
            vk::ImageLayout new_image_layout,
            uint32_t mipmap_level,
            uint32_t array_layer
        ) {
            const vk::ImageMemoryBarrier image_barrier(
                src_access,
                dst_access,
                old_image_layout,
                new_image_layout,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                vk_image,
                vk::ImageSubresourceRange( // sub resource
                        vk_aspect, // aspect
                        mipmap_level, // base mipmap
                        1, // mipmap count
                        array_layer, // base layer
                        1 // layer count
                    )
            );

            cmd_buffer.pipelineBarrier(
                src_pipeline_stage,
                dst_pipeline_stage,
                {},
                {},
                {},
                {image_barrier}
            );
        };

        TransferImageLayoutDesc layout_transfer_barrier{
            typed_image,
            vk::ImageLayout::eTransferDstOptimal,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            vk::AccessFlagBits::eTransferWrite,
        };

        //transfer whole image layout
        TransferImageLayout(std::span(&layout_transfer_barrier, 1));

        for (uint32_t array_index = 0; array_index < image_desc.extent.z; ++array_index) {
            int32_t mipWidth = image_desc.extent.x;
            int32_t mipHeight = image_desc.extent.y;

            for (uint32_t mipmap_index = 1; mipmap_index < image_desc.mipmap_levels; ++mipmap_index) {
                barrier(
                    vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eTransferRead,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::ImageLayout::eTransferSrcOptimal,
                    mipmap_index - 1,
                    array_index
                );

                vk::ImageBlit image_blit(
                    vk::ImageSubresourceLayers(
                        typed_image->GetAspect(),
                        mipmap_index - 1,
                        array_index,
                        1
                    ),
                    {vk::Offset3D{}, {mipWidth, mipHeight, 1}},
                    vk::ImageSubresourceLayers(
                        typed_image->GetAspect(),
                        mipmap_index,
                        array_index,
                        1
                    ),
                    {vk::Offset3D{}, {
                        mipWidth > 1 ? mipWidth / 2 : 1,
                        mipHeight > 1 ? mipHeight / 2 : 1,
                        1}}
                );

                command_buffer.blitImage(
                    typed_image->GetImage(),
                    vk::ImageLayout::eTransferSrcOptimal,
                    typed_image->GetImage(),
                    vk::ImageLayout::eTransferDstOptimal,
                    {image_blit},
                    vk::Filter::eLinear
                );

                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
            }

            barrier(
                vk::AccessFlagBits::eTransferWrite,
                {},
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eTransferSrcOptimal,
                image_desc.mipmap_levels - 1,
                array_index
            );
        }

        //old image layout
        typed_image->m_image_layout = vk::ImageLayout::eTransferSrcOptimal;

        layout_transfer_barrier = {
            typed_image,
            typed_image->GetIdealImageLayout(),
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eVertexShader,
            vk::AccessFlagBits::eTransferRead,
            {}
        };

        //transfer whole images, layout
        TransferImageLayout(std::span(&layout_transfer_barrier, 1));
    }

    void CommandBuffer::BindVertexBuffer(const DnmGLLite::Buffer *buffer, uint64_t offset) {
        command_buffer.bindVertexBuffers(
            0, 
            {static_cast<const Vulkan::Buffer *>(buffer)->GetBuffer()}, 
            {offset});
    }

    void CommandBuffer::BindIndexBuffer(const DnmGLLite::Buffer *buffer, uint64_t offset, DnmGLLite::IndexType index_type) {
        command_buffer.bindIndexBuffer(
            static_cast<const Vulkan::Buffer *>(buffer)->GetBuffer(), 
            offset, 
            static_cast<vk::IndexType>(index_type));
    }

    void CommandBuffer::UploadData(const DnmGLLite::Buffer *buffer, const void* data, uint32_t size, uint32_t offset) {
        const auto *typed_buffer = static_cast<const Vulkan::Buffer *>(buffer);

        if (size < 65536) {
            command_buffer.updateBuffer(typed_buffer->GetBuffer(), offset, size, data);
            return;
        }

        //No problem, the Vulkan object is destroyed at the start of ExecuteCommands() or Render()
        const Vulkan::Buffer staging_buffer(*VulkanContext, {
            .size = size,
            .memory_host_access = MemoryHostAccess::eWrite,
            .memory_type = MemoryType::eAuto,
            .buffer_flags = {},
        });

        memcpy(staging_buffer.GetMappedPtr() + offset, data, size);

        CopyBufferToBuffer({
            .src_buffer = &staging_buffer,
            .dst_buffer = typed_buffer,
            .src_offset = offset,
            .dst_offset = 0,
            .copy_size = size,
        });
    }

    void CommandBuffer::UploadData(DnmGLLite::Image *image, const ImageSubresource& subresource, const void* data, uint32_t size, Uint3 offset) {
        //No problem, the Vulkan object is destroyed at the start of ExecuteCommands() or Render()
        const Vulkan::Buffer staging_buffer(*VulkanContext, {
            .size = size,
            .memory_host_access = MemoryHostAccess::eWrite,
            .memory_type = MemoryType::eAuto,
            .buffer_flags = {},
        });

        memcpy(staging_buffer.GetMappedPtr(), data, size);

        CopyBufferToImage({
            .src_buffer = &staging_buffer,
            .dst_image = image,
            .image_subresource = subresource,
            .buffer_offset = 0,
            .buffer_row_lenght = image->GetDesc().extent.x,
            .buffer_image_height = image->GetDesc().extent.y,
            .image_offset = {offset.x, offset.y, offset.z},
            .image_extent = image->GetDesc().extent,
        });
    }

    void CommandBuffer::BeginRendering(
            const DnmGLLite::GraphicsPipeline *pipeline, 
            std::span<const ColorFloat> color_clear_values, 
            std::optional<DepthStencilClearValue> depth_stencil_clear_value) {
        const auto* typed_pipeline = static_cast<const Vulkan::GraphicsPipeline *>(pipeline);
        const auto renderpass = typed_pipeline->GetRenderpass();
        const auto framebuffer = typed_pipeline->GetFramebuffer();

        ResourceBarrier(
            typed_pipeline->GetBufferResourceAccessInfo(), 
            typed_pipeline->GetImageResourceAccessInfo());

        ProcressDeferTranslateImageLayout();

        command_buffer.bindDescriptorSets(
                        vk::PipelineBindPoint::eGraphics, 
                        typed_pipeline->GetPipelineLayout(),
                        0,
                        typed_pipeline->GetDstSets(),
                        {});

        command_buffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics, 
            static_cast<const Vulkan::GraphicsPipeline*>(pipeline)->GetPipeline()
        );

        std::vector<vk::ClearValue> clear_values{};
        clear_values.reserve(color_clear_values.size() + 1);
        for (const auto value : color_clear_values) {
            clear_values.emplace_back(
                vk::ClearColorValue(std::array{value.r, value.g, value.b, value.a})
            );
        }
        if (depth_stencil_clear_value.has_value()) {
            clear_values.emplace_back(
                vk::ClearDepthStencilValue(depth_stencil_clear_value->depth, depth_stencil_clear_value->stencil)
            );
        }

        vk::RenderPassBeginInfo begin_desc{};
        begin_desc.setRenderPass(renderpass)
                    .setFramebuffer(framebuffer)
                    .setClearValues(clear_values)
                    .setRenderArea({{}, typed_pipeline->GetRenderArea()})
                    ;

        command_buffer.beginRenderPass(
            begin_desc, 
            vk::SubpassContents::eInline);
    }

    void CommandBuffer::EndRendering(const DnmGLLite::GraphicsPipeline *pipeline) {
        command_buffer.endRenderPass();

        // translate user image's layouts
        {
            const auto* typed_pipeline = static_cast<const Vulkan::GraphicsPipeline *>(pipeline);
            auto& user_color_images = typed_pipeline->GetUserColorAttachments();
            auto* user_depth_stencil_image = typed_pipeline->GetUserDepthStencilAttachment();
    
            std::vector<TransferImageLayoutDesc> transfer_image_layout;
            transfer_image_layout.reserve(user_color_images.size() + 2);
    
            for (auto& image : user_color_images) {
                transfer_image_layout.emplace_back(
                    image,
                    GetIdealImageLayout(image->GetDesc().usage_flags),
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    vk::AccessFlagBits::eColorAttachmentWrite,
                    vk::AccessFlagBits{}
                );
            }
            
            if (user_depth_stencil_image != nullptr) {
                transfer_image_layout.emplace_back(
                    user_depth_stencil_image,
                    GetIdealImageLayout(user_depth_stencil_image->GetDesc().usage_flags),
                    vk::PipelineStageFlagBits::eLateFragmentTests,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                    vk::AccessFlagBits{}
                );
            }
    
            TransferImageLayout(transfer_image_layout);
        }
    }

    void CommandBuffer::ProcressDeferTranslateImageLayout() {
        if (!m_defer_translate_image_layout.size()) {
            return;
        }

        std::vector<TransferImageLayoutDesc> image_layout_transfer_desc;
        image_layout_transfer_desc.reserve(m_defer_translate_image_layout.size());

        for (auto* image : m_defer_translate_image_layout) {
            const auto dst_access_flag = 
                image->GetIdealImageLayout() == vk::ImageLayout::eTransferSrcOptimal ? vk::AccessFlagBits::eTransferRead : vk::AccessFlagBits::eTransferWrite;
            image_layout_transfer_desc.emplace_back(
                image,
                image->GetIdealImageLayout(),
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eVertexShader,
                dst_access_flag,
                vk::AccessFlagBits::eShaderRead
            );
        }
        TransferImageLayout(image_layout_transfer_desc);
        m_defer_translate_image_layout.clear();
    }
}