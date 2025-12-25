#pragma once

#include "DnmGLLite/Vulkan/Context.hpp"

namespace DnmGLLite::Vulkan {
    class Sampler final : public DnmGLLite::Sampler {
    public:
        Sampler(DnmGLLite::Vulkan::Context& context, const DnmGLLite::SamplerDesc& desc);
        ~Sampler() {
            const auto sampler = m_sampler;
            VulkanContext->DeleteObject(
                [sampler] (vk::Device device, [[maybe_unused]] VmaAllocator allocator) -> void {
                    device.destroy(sampler);
                });
        }

        [[nodiscard]] vk::Sampler GetSampler() const { return m_sampler; }
    private:
        vk::Sampler m_sampler;
    };
}