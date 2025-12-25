#include "DnmGLLite/Vulkan/ResourceManager.hpp"
#include "DnmGLLite/Vulkan/Shader.hpp"
#include "DnmGLLite/Vulkan/Image.hpp"
#include "DnmGLLite/Vulkan/Buffer.hpp"
#include "DnmGLLite/Vulkan/Sampler.hpp"

namespace DnmGLLite::Vulkan {
    ResourceManager::ResourceManager(DnmGLLite::Vulkan::Context& ctx, std::span<const DnmGLLite::Shader*> shaders)
        : DnmGLLite::ResourceManager(ctx, shaders) {
        const auto* vk_ctx = VulkanContext;

        std::vector<std::vector<vk::DescriptorSetLayoutBinding>> set_bindings{};

        for (const auto* shader : shaders) {
            const auto* typed_shader = reinterpret_cast<const Vulkan::Shader*>(shader);
            for (const auto shader_dst_set : typed_shader->GetDescriptorSets()) {
                if (set_bindings.size() <= shader_dst_set.idx)
                    set_bindings.resize(shader_dst_set.idx + 1);

                auto& bindings = set_bindings[shader_dst_set.idx];

                for (const auto& shader_binding : shader_dst_set.bindings) {
                    auto it = std::ranges::find_if(
                            bindings,
                            [&shader_binding] (vk::DescriptorSetLayoutBinding& binding) {
                            return binding.binding == shader_binding.binding;
                        });

                    //if this binding exists
                    if (it != bindings.end()) {
                        it->stageFlags |= typed_shader->GetStage();
                        continue;
                    }

                    bindings.emplace_back(
                        shader_binding.binding,
                        shader_binding.type,
                        shader_binding.descriptor_count,
                        typed_shader->GetStage(),
                        nullptr
                    );
                }
            }
        }

        for (const auto& bindings : set_bindings) {
            m_dst_set_layouts.emplace_back(vk_ctx->GetDevice().createDescriptorSetLayout(
                vk::DescriptorSetLayoutCreateInfo{}
                .setBindings(bindings)
                .setFlags({})
            ));
        }

        {
            vk::DescriptorSetAllocateInfo alloc_info;
            alloc_info.setSetLayouts(m_dst_set_layouts)
                        .setDescriptorPool(vk_ctx->GetDescriptorPool())
                        ;

            m_sets = vk_ctx->GetDevice().allocateDescriptorSets(alloc_info);
        }
    }

    void ResourceManager::SetResourceAsBuffer(std::span<const BufferResource> update_resource) {
        const auto supported_features = VulkanContext->GetSupportedFeatures();

        std::vector<Context::InternalBufferResource> updates;
        std::vector<Context::InternalBufferResource> defer_updates;

        if (supported_features.uniform_buffer_update_after_bind
            && supported_features.storage_buffer_update_after_bind) {
            updates.reserve(update_resource.size());
        }
        else if (supported_features.uniform_buffer_update_after_bind
            || supported_features.storage_buffer_update_after_bind) {
            updates.reserve(update_resource.size());
            defer_updates.reserve(update_resource.size());
        }
        else {
            defer_updates.reserve(update_resource.size());
        }
        
        const auto context_state = VulkanContext->GetContextState();
        const bool context_state_is_ideal = context_state == Vulkan::ContextState::eNone || context_state == Vulkan::ContextState::eCommandBufferRecording;

        for (const auto& res : update_resource) {
            bool update_now = context_state_is_ideal;

            if (res.type == BufferResourceType::eUniformBuffer && !update_now) {
                update_now = supported_features.uniform_buffer_update_after_bind;
            }
            else if (res.type == BufferResourceType::eStorageBuffer) {
                update_now = supported_features.storage_buffer_update_after_bind;
            }

            auto& internal_res
                = update_now ? updates.emplace_back() : defer_updates.emplace_back();

            internal_res.type = (res.type == BufferResourceType::eUniformBuffer) ? vk::DescriptorType::eUniformBuffer : vk::DescriptorType::eStorageBuffer;
            internal_res.buffer = static_cast<const Vulkan::Buffer*>(res.buffer)->GetBuffer();
            internal_res.array_element = res.array_element;
            internal_res.binding = res.binding;
            internal_res.offset = res.offset;
            internal_res.size = res.size;
            internal_res.set = m_sets[res.set];
        }
        if (!defer_updates.empty()) {
            VulkanContext->DeferResourceUpdate(defer_updates);
        }
        if (!updates.empty()) {
            std::vector<vk::WriteDescriptorSet> writes(updates.size());
            std::vector<vk::DescriptorBufferInfo> buffer_infos(updates.size());

            uint32_t i{};
            for (const auto& res : updates) {
                VulkanContext->ProcessResource(res, buffer_infos[i], writes[i]);
                ++i;
            }

            VulkanContext->GetDevice().updateDescriptorSets(writes, {});
        }
    }

    void ResourceManager::SetResourceAsImage(std::span<const ImageResource> update_resource) {
        const auto supported_features = VulkanContext->GetSupportedFeatures();

        std::vector<Context::InternalImageResource> updates;
        std::vector<Context::InternalImageResource> defer_updates;

        if (supported_features.storage_image_update_after_bind) {
            updates.reserve(update_resource.size());
        }
        else {
            defer_updates.reserve(update_resource.size());
        }

        const auto context_state = VulkanContext->GetContextState();
        const bool context_state_is_ideal = context_state == Vulkan::ContextState::eNone || context_state == Vulkan::ContextState::eCommandBufferRecording;

        for (const auto& res : update_resource) {
            auto& internal_res
                = supported_features.storage_image_update_after_bind || context_state_is_ideal ? 
                updates.emplace_back() : defer_updates.emplace_back();
            internal_res.image_view = static_cast<Vulkan::Image*>(res.image)->CreateGetImageView(res.subresource);
            internal_res.array_element = res.array_element;
            internal_res.binding = res.binding;
            internal_res.set = m_sets[res.set];
        }
        if (!defer_updates.empty()) {
            VulkanContext->DeferResourceUpdate(defer_updates);
        }
        if (!updates.empty()) {
            std::vector<vk::WriteDescriptorSet> writes(updates.size());
            std::vector<vk::DescriptorImageInfo> buffer_infos(updates.size());

            uint32_t i{};
            for (const auto& res : updates) {
                VulkanContext->ProcessResource(res, buffer_infos[i], writes[i]);
                ++i;
            }

            VulkanContext->GetDevice().updateDescriptorSets(writes, {});
        }
    }

    void ResourceManager::SetResourceAsTexture(std::span<const TextureResource> update_resource) {
        const auto supported_features = VulkanContext->GetSupportedFeatures();

        std::vector<Context::InternalTextureResource> updates;
        std::vector<Context::InternalTextureResource> defer_updates;

        if (supported_features.storage_image_update_after_bind) {
            updates.reserve(update_resource.size());
        }
        else {
            defer_updates.reserve(update_resource.size());
        }

        const auto context_state = VulkanContext->GetContextState();
        const bool context_state_is_ideal = context_state == Vulkan::ContextState::eNone || context_state == Vulkan::ContextState::eCommandBufferRecording;

        uint32_t i{};
        for (const auto& res : update_resource) {
            auto& internal_res
                = supported_features.sampled_image_update_after_bind || context_state_is_ideal
                ? updates.emplace_back() : defer_updates.emplace_back();
            internal_res.image_view = static_cast<Vulkan::Image*>(res.image)->CreateGetImageView(res.subresource);
            internal_res.sampler = static_cast<Vulkan::Sampler*>(res.sampler)->GetSampler();
            internal_res.array_element = res.array_element;
            internal_res.image_layout = GetIdealImageLayout(res.image->GetDesc().usage_flags);
            internal_res.binding = res.binding;
            internal_res.set = m_sets[res.set];
            ++i;
        }
        if (!defer_updates.empty()) {
            VulkanContext->DeferResourceUpdate(defer_updates);
        }
        if (!updates.empty()) {
            std::vector<vk::WriteDescriptorSet> writes(updates.size());
            std::vector<vk::DescriptorImageInfo> buffer_infos(updates.size());

            uint32_t i{};
            for (const auto& res : updates) {
                VulkanContext->ProcessResource(res, buffer_infos[i], writes[i]);
                ++i;
            }

            VulkanContext->GetDevice().updateDescriptorSets(writes, {});
        }
    }

    std::vector<vk::DescriptorSetLayout> ResourceManager::GetDescriptorLayouts(std::span<const Vulkan::Shader *> shaders) const noexcept {
        if (m_dst_set_layouts.size() == 0) {
            return {};
        }
        if (m_shaders.size() == 1) {
            return m_dst_set_layouts;
        }

        std::vector<vk::DescriptorSetLayout> set_layouts;
        set_layouts.reserve(4);
        for (const auto* shader : shaders) {
            for (const auto& [i, _] : shader->GetDescriptorSets()) {
                set_layouts.push_back(m_dst_set_layouts[i]);
            }
        }
        return std::move(set_layouts);
    }

    std::vector<vk::DescriptorSet> ResourceManager::GetDescriptorSets(std::span<const Vulkan::Shader *> shaders) const noexcept {
        if (m_sets.size() == 0) {
            return {};
        }
        if (m_shaders.size() == 1) {
            return m_sets;
        }

        std::vector<vk::DescriptorSet> sets;
        sets.reserve(4);
        for (const auto* shader : shaders) {
            for (const auto& [i, _] : shader->GetDescriptorSets()) {
                sets.push_back(m_sets[i]);
            }
        }
        return std::move(sets);
    }
} // namespace DnmGLLite::Vulkan