#pragma once

#include "DnmGLLite/Vulkan/Context.hpp"

namespace DnmGLLite::Vulkan {
    struct TransferImageLayoutDesc {
        Vulkan::Image* image;
        vk::ImageLayout new_image_layout;
        vk::PipelineStageFlags src_pipeline_stages;
        vk::PipelineStageFlags dst_pipeline_stages;
        vk::AccessFlags src_access;
        vk::AccessFlags dst_access;
    };

    struct TransferImageLayoutNativeDesc {
        vk::Image image;
        vk::ImageAspectFlags image_aspect;
        vk::ImageLayout old_image_layout;
        vk::ImageLayout new_image_layout;
        vk::PipelineStageFlags src_pipeline_stages;
        vk::PipelineStageFlags dst_pipeline_stages;
        vk::AccessFlags src_access;
        vk::AccessFlags dst_access;
    };

    class CommandBuffer final : public DnmGLLite::CommandBuffer {
    public:
        CommandBuffer(Vulkan::Context& context);
        ~CommandBuffer() noexcept {
            VulkanContext
                ->GetDevice().freeCommandBuffers(VulkanContext->GetCommandPool(), command_buffer);
        }
    
        void Begin() override;
        void End() override;

        //i hate renderpasses
        void BeginRendering(
            const DnmGLLite::GraphicsPipeline *pipeline,
            std::span<const DnmGLLite::ColorFloat> color_clear_values, 
            std::optional<DnmGLLite::DepthStencilClearValue> depth_stencil_clear_value) override;
        void EndRendering(const DnmGLLite::GraphicsPipeline *pipeline) override;

        void UploadData(DnmGLLite::Image *image, const ImageSubresource& subresource, const void* data, uint32_t size, Uint3 offset) override;
        void UploadData(const DnmGLLite::Buffer *buffer, const void* data, uint32_t size, uint32_t offset) override;
    
        void CopyImageToBuffer(const DnmGLLite::ImageToBufferCopyDesc& desc) override;
        void CopyImageToImage(const DnmGLLite::ImageToImageCopyDesc& desc) override;
        void CopyBufferToImage(const DnmGLLite::BufferToImageCopyDesc& desc) override;
        void CopyBufferToBuffer(const DnmGLLite::BufferToBufferCopyDesc& desc) override;
    
        void TransferImageLayout(std::span<const TransferImageLayoutDesc> desc) const;
        void TransferImageLayout(std::span<const TransferImageLayoutNativeDesc> desc) const;
    
        void Dispatch(uint32_t x = 1, uint32_t y = 1, uint32_t z = 1);

        void BindPipeline(const DnmGLLite::ComputePipeline* pipeline) override;

        void GenerateMipmaps(DnmGLLite::Image* image) override;
    
        void BindVertexBuffer(const DnmGLLite::Buffer* buffer, uint64_t offset) override;
        void BindIndexBuffer(const DnmGLLite::Buffer* buffer, uint64_t offset, DnmGLLite::IndexType index_type) override;
    
        void Draw(uint32_t vertex_count, uint32_t instance_count) override;
        void DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) override;
    
        void PushConstant(const DnmGLLite::GraphicsPipeline* pipeline, DnmGLLite::ShaderStageFlags pipeline_stage, uint32_t offset, uint32_t size, const void *ptr) override;
        void PushConstant(const DnmGLLite::ComputePipeline* pipeline, DnmGLLite::ShaderStageFlags pipeline_stage, uint32_t offset, uint32_t size, const void *ptr) override;

        void SetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) override;
        void SetScissor(Uint2 extent, Uint2 offset) override;
    private:
        void TransferImageLayoutDefaultVk(std::span<const TransferImageLayoutNativeDesc> descs) const;
        void TransferImageLayoutSync2(std::span<const TransferImageLayoutNativeDesc> descs) const;
        
        void ResourceBarrier(ResourceAccessInfo res_access_info, ResourceAccessInfo image_access_info);
        
        void AddImageForDeferTranslateLayout(Vulkan::Image *image);
        void ProcressDeferTranslateImageLayout();
        
        vk::CommandBuffer command_buffer;

        ResourceAccessInfo m_buffer_resource_access_info;
        ResourceAccessInfo m_image_resource_access_info;

        //procress in BindPipeline or begin pipeline
        std::unordered_set<Vulkan::Image *> m_defer_translate_image_layout;

        friend Vulkan::Context;
    };
    
    inline void CommandBuffer::Begin() {
        command_buffer.reset();
        command_buffer.begin(vk::CommandBufferBeginInfo{});
    }
    
    inline void CommandBuffer::End() {
        ProcressDeferTranslateImageLayout();
        command_buffer.end();

        m_buffer_resource_access_info = {};
        m_image_resource_access_info = {};
    }
    
    inline void CommandBuffer::Draw(uint32_t vertex_count, uint32_t instance_count) {
        command_buffer.draw(vertex_count, instance_count, 0, 0);
    }
    
    inline void CommandBuffer::DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) {
        command_buffer.drawIndexed(index_count, instance_count, 0, vertex_offset, 0);
    }

    inline void CommandBuffer::SetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) {
        command_buffer.setViewport(0, {vk::Viewport{}
            .setMinDepth(max_depth).setMinDepth(max_depth)
            .setHeight(extent.y).setWidth(extent.x)
            .setX(offset.x).setY(offset.y)
        });
    }

    inline void CommandBuffer::SetScissor(Uint2 extent, Uint2 offset) {
        command_buffer.setScissor(0, { vk::Rect2D{}
            .setOffset({static_cast<int32_t>(offset.x), static_cast<int32_t>(offset.y)})
            .setExtent({extent.x, extent.y})
        });
    }

    inline void CommandBuffer::TransferImageLayout(
        std::span<const TransferImageLayoutNativeDesc> descs) const {
        if (!descs.size()) {
            return;
        } 
        else if (VulkanContext->GetSupportedFeatures().sync2) {
            TransferImageLayoutSync2(descs);
        }
        else {
            TransferImageLayoutDefaultVk(descs);
        }
    }

    inline void CommandBuffer::ResourceBarrier(ResourceAccessInfo buffer_access_info, ResourceAccessInfo image_access_info) {
        const bool need_buffer_barrier = 
            (m_buffer_resource_access_info.access == ResourceAccessBit::eRead 
            || m_buffer_resource_access_info.access.None())
            && buffer_access_info.access.Any();

        const bool need_image_barrier = 
            (m_image_resource_access_info.access == ResourceAccessBit::eRead 
            || m_image_resource_access_info.access.None())
            && image_access_info.access.Any();

        if (!(need_buffer_barrier || need_image_barrier))
            return;

        vk::MemoryBarrier barrier{};
        vk::PipelineStageFlags src_pipeline_flags{};
        vk::PipelineStageFlags dst_pipeline_flags{};

        // no barrier for buffer
        if (need_buffer_barrier) {
            barrier.srcAccessMask |= m_buffer_resource_access_info.GetVkAccessFlags();
            barrier.dstAccessMask |= buffer_access_info.GetVkAccessFlags();
            src_pipeline_flags |= m_buffer_resource_access_info.stages;
            dst_pipeline_flags |= buffer_access_info.stages;
        }

        // no barrier for image
        if ((m_image_resource_access_info.access == ResourceAccessBit::eRead 
            || m_image_resource_access_info.access.None())
            && image_access_info.access.Any()) {
            barrier.srcAccessMask |= m_image_resource_access_info.GetVkAccessFlags();
            barrier.dstAccessMask |= image_access_info.GetVkAccessFlags();
            src_pipeline_flags |= m_image_resource_access_info.stages;
            dst_pipeline_flags |= image_access_info.stages;
        }

        command_buffer.pipelineBarrier(
                src_pipeline_flags,
                dst_pipeline_flags,
                {}, 
                barrier,
                {}, 
                {});

        m_buffer_resource_access_info = buffer_access_info;
        m_image_resource_access_info = image_access_info;
    }

    inline void CommandBuffer::Dispatch(uint32_t x, uint32_t y, uint32_t z) {
        command_buffer.dispatch(x, y, z);
    }

    inline void CommandBuffer::AddImageForDeferTranslateLayout(Vulkan::Image *image) {
        m_defer_translate_image_layout.emplace(image);
    }
}