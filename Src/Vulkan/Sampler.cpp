#include "DnmGLLite/Vulkan/Sampler.hpp"

namespace DnmGLLite::Vulkan {
    Sampler::Sampler(DnmGLLite::Vulkan::Context& context, const DnmGLLite::SamplerDesc& desc)
        : DnmGLLite::Sampler(context, desc) {

        vk::Filter filter = vk::Filter::eLinear;
        bool anisotropy = false;
        float anisotropy_level = 1.f;

        switch (desc.filter) {
        case SamplerFilter::eNearest: {
            filter = vk::Filter::eNearest; break;
        }
        case SamplerFilter::eLinear: break;
        default: anisotropy = true; anisotropy_level = static_cast<float>(desc.filter);
        }

        vk::SamplerCreateInfo create_info{};
        create_info.setBorderColor(vk::BorderColor::eFloatTransparentBlack)
                    .setMipmapMode(static_cast<vk::SamplerMipmapMode>(desc.mipmap_mode))
                    .setMinFilter(filter)
                    .setMagFilter(filter)
                    .setAnisotropyEnable(anisotropy)
                    .setMaxAnisotropy(anisotropy_level)
                    .setCompareEnable(
                        (desc.compare_op == CompareOp::eAlways || desc.compare_op == CompareOp::eNever) ? vk::False : vk::True)
                    .setCompareOp(static_cast<vk::CompareOp>(desc.compare_op))
                    .setMinLod(0)
                    .setMaxLod(VK_LOD_CLAMP_NONE)
                    .setAddressModeU(static_cast<vk::SamplerAddressMode>(desc.address_mode_u))
                    .setAddressModeV(static_cast<vk::SamplerAddressMode>(desc.address_mode_v))
                    .setAddressModeW(static_cast<vk::SamplerAddressMode>(desc.address_mode_w))
                    .setMipLodBias(0.f)
                    ;

        m_sampler = context.GetDevice().createSampler(create_info);
    }
}