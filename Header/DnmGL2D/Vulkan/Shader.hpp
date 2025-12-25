#pragma once

#include "DnmGLLite/Vulkan/Context.hpp"

namespace DnmGLLite::Vulkan {
    class Shader final : public DnmGLLite::Shader {
        struct BindingInfo {
            uint32_t binding;
            uint32_t descriptor_count;
            vk::DescriptorType type;
        };

        struct DescriptorSetInfo {
            uint32_t idx;
            std::vector<BindingInfo> bindings;
        };

        struct PushConstant {
            uint32_t size;
            uint32_t offset;
        };
    public:
        Shader(Vulkan::Context& context, const std::filesystem::path& path);
        ~Shader() {
            const auto shader_module = m_shader_module;
            VulkanContext->DeleteObject(
                [shader_module] (vk::Device device, [[maybe_unused]] VmaAllocator allocator) -> void {
                    device.destroy(shader_module);
                });
        }

        [[nodiscard]] vk::ShaderStageFlagBits GetStage() const { return m_stage; }
        [[nodiscard]] vk::ShaderModule GetShaderModule() const { return m_shader_module; }

        [[nodiscard]] const auto& GetDescriptorSets() const { return m_descriptor_sets; }
        [[nodiscard]] const auto& GetPushConstants() const { return m_push_constant; }

        [[nodiscard]] auto GetBufferResourceAccessInfo() const { return m_buffer_resource_access_info; }
        [[nodiscard]] auto GetImageResourceAccessInfo() const { return m_image_resource_access_info; }
    private:
        vk::ShaderModule m_shader_module{ VK_NULL_HANDLE };
        
        std::vector<DescriptorSetInfo> m_descriptor_sets;
        std::optional<PushConstant> m_push_constant;
        
        ResourceAccessInfo m_buffer_resource_access_info{};
        ResourceAccessInfo m_image_resource_access_info{};
        vk::ShaderStageFlagBits m_stage = vk::ShaderStageFlagBits::eAll;
    };
}