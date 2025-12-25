#pragma once

#include "DnmGLLite/Vulkan/Context.hpp"
#include "DnmGLLite/Vulkan/Image.hpp"

namespace DnmGLLite::Vulkan {
    //TODO: store unordered access resources data for sync
    class GraphicsPipeline final : public DnmGLLite::GraphicsPipeline {
    public:
        GraphicsPipeline(Vulkan::Context& context, const DnmGLLite::GraphicsPipelineDesc& desc) noexcept;
        ~GraphicsPipeline() noexcept;

        void SetAttachments(
            std::span<const DnmGLLite::RenderAttachment> attachments, 
            const std::optional<DnmGLLite::RenderAttachment> &depth_stencil_attachment,
            Uint2 extent) override;

        [[nodiscard]] auto GetPipeline() const { return m_pipeline; }
        [[nodiscard]] auto GetPipelineLayout() const { return m_pipeline_layout; }
        [[nodiscard]] auto GetRenderpass() const { return m_renderpass; }
        [[nodiscard]] auto GetRenderArea() const { return m_render_area; }
        [[nodiscard]] auto& GetUserColorAttachments() const { return m_user_color_attachments; }
        [[nodiscard]] auto* GetUserDepthStencilAttachment() const { return m_user_depth_stencil_attachment; }
        [[nodiscard]] auto HasDepthAttachment() const { return m_has_depth_attachment; }
        [[nodiscard]] auto HasStencilAttachment() const { return m_has_stencil_attachment; }
        [[nodiscard]] auto HasMsaa() const { return m_desc.msaa != SampleCount::e1; }
        [[nodiscard]] auto GetColorAttachmentCount() const { return m_desc.presenting ? 1 : m_desc.color_attachment_formats.size(); }
        [[nodiscard]] auto GetFramebuffer() const { 
            return m_desc.presenting ? m_swapchain_framebuffers[VulkanContext->GetImageIndex()] : m_framebuffer;
        }
        [[nodiscard]] auto GetBufferResourceAccessInfo() const { return m_buffer_resource_access_info; }
        [[nodiscard]] auto GetImageResourceAccessInfo() const { return m_image_resource_access_info; }
    private:
        void CreateRenderpass() noexcept;
        void DestroyFramebuffer() const noexcept;

        vk::ImageView CreateInternalResource(Uint2 extent, ImageUsageFlags usage, Format format, SampleCount sample_count) noexcept;

        vk::Pipeline m_pipeline;
        vk::PipelineLayout m_pipeline_layout;

        //i hate renderpasses
        vk::RenderPass m_renderpass;
        vk::Framebuffer m_framebuffer;
        vk::Extent2D m_render_area;

        std::vector<Vulkan::Image *> m_user_color_attachments{}; 
        Vulkan::Image *m_user_depth_stencil_attachment{};

        std::vector<std::unique_ptr<Vulkan::Image>> m_attachments{};
        std::vector<vk::Framebuffer> m_swapchain_framebuffers{};

        ResourceAccessInfo m_buffer_resource_access_info{};
        ResourceAccessInfo m_image_resource_access_info{};

        bool m_has_depth_attachment : 1{};
        bool m_has_stencil_attachment : 1{};
    };

    //TODO: store unordered access resources data for sync
    class ComputePipeline final : public DnmGLLite::ComputePipeline  {
    public:
        ComputePipeline(Vulkan::Context& context, const DnmGLLite::ComputePipelineDesc& desc) noexcept;
        ~ComputePipeline() noexcept;
        
        [[nodiscard]] auto GetPipeline() const { return m_pipeline; }
        [[nodiscard]] auto GetPipelineLayout() const { return m_pipeline_layout; }

        [[nodiscard]] auto GetBufferResourceAccessInfo() const { return m_buffer_resource_access_info; }
        [[nodiscard]] auto GetImageResourceAccessInfo() const { return m_image_resource_access_info; }
    private:
        ResourceAccessInfo m_buffer_resource_access_info{};
        ResourceAccessInfo m_image_resource_access_info{};
        
        vk::Pipeline m_pipeline;
        vk::PipelineLayout m_pipeline_layout;
    };

    inline ComputePipeline::~ComputePipeline() noexcept {
        VulkanContext->DeleteObject(
            [
                pipeline = m_pipeline, 
                pipeline_layout = m_pipeline_layout
            ] (vk::Device device, [[maybe_unused]] VmaAllocator allocator) noexcept -> void {
                device.destroy(pipeline_layout);
                device.destroy(pipeline);
            });
    }

    inline GraphicsPipeline::~GraphicsPipeline() noexcept {
        VulkanContext->DeleteObject(
            [
                pipeline = m_pipeline, 
                pipeline_layout = m_pipeline_layout,
                renderpass = m_renderpass,
                framebuffer = m_framebuffer,
                swapchain_framebuffers = m_swapchain_framebuffers
            ] (vk::Device device, [[maybe_unused]] VmaAllocator allocator) noexcept -> void {
                device.destroy(pipeline_layout);
                device.destroy(pipeline);
                device.destroy(renderpass);
                device.destroy(framebuffer);
                for (const auto swapchain_framebuffer : swapchain_framebuffers)
                    device.destroy(swapchain_framebuffer);
            });
    }

    inline void GraphicsPipeline::DestroyFramebuffer() const noexcept {
        if (m_framebuffer == VK_NULL_HANDLE) {
            return;
        }
        VulkanContext->DeleteObject(
            [
                framebuffer = m_framebuffer
            ] (vk::Device device, [[maybe_unused]] VmaAllocator allocator) -> void {
                device.destroy(framebuffer);
            });
    }
}