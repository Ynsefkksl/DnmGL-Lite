#pragma once

#include "DnmGLLite/Vulkan/Context.hpp"

namespace DnmGLLite::Vulkan {
    class ResourceManager final : public DnmGLLite::ResourceManager {
    public:
        ResourceManager(DnmGLLite::Vulkan::Context& context, std::span<const DnmGLLite::Shader *> shaders);
        ~ResourceManager();

        void SetResourceAsBuffer(std::span<const BufferResource> update_resource) override;
        void SetResourceAsImage(std::span<const ImageResource> update_resource) override;
        void SetResourceAsTexture(std::span<const TextureResource> update_resource) override;

        constexpr void BindSets(vk::CommandBuffer command_buffer, vk::PipelineBindPoint bind_point, vk::PipelineLayout pipeline_layout) noexcept;

        [[nodiscard]] std::vector<vk::DescriptorSetLayout> GetDescriptorLayouts(std::span<const Vulkan::Shader *> shaders) const noexcept;
        [[nodiscard]] constexpr std::span<const vk::DescriptorSetLayout> GetDescriptorLayouts() const noexcept { return m_dst_set_layouts; }
        [[nodiscard]] std::vector<vk::DescriptorSet> GetDescriptorSets(std::span<const Vulkan::Shader *> shaders) const noexcept;
        [[nodiscard]] constexpr std::span<const vk::DescriptorSet> GetDescriptorSets() const noexcept { return m_sets; }
    private:
        std::vector<vk::DescriptorSet> m_sets;
        std::vector<vk::DescriptorSetLayout> m_dst_set_layouts;

        vk::DescriptorSet m_placeholder_set;
    };

    inline ResourceManager::~ResourceManager() {
        const auto dst_set_layouts = std::move(m_dst_set_layouts);
        VulkanContext->DeleteObject(
            [dst_set_layouts] (vk::Device device, [[maybe_unused]] VmaAllocator allocator) -> void {
                for (auto set_layout: dst_set_layouts) {
                    device.destroy(set_layout);
                }
            });
    }

    inline constexpr void ResourceManager::BindSets(vk::CommandBuffer command_buffer, vk::PipelineBindPoint bind_point, vk::PipelineLayout pipeline_layout) noexcept {
        command_buffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, 
            pipeline_layout,
            0,
            GetDescriptorSets(),
            {});
    }
}