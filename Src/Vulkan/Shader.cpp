#include "DnmGLLite/Vulkan/Shader.hpp"

#include <spirv_reflect.h>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace DnmGLLite::Vulkan {
    static constexpr std::vector<uint32_t> LoadShaderFile(const std::filesystem::path& filePath) {
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);

        const auto fileSize = file.tellg();
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t)); //SPIR-V consists of 32 bit words 

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        file.close();

        return buffer;
    }

    static void FillResAccessInfo(const SpvReflectDescriptorBinding& binding, ResourceAccessInfo& buffer_res_access_info, ResourceAccessInfo& image_res_access_info) {
        if (binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE 
        || binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE ) {
            switch (binding.resource_type) {
                case SPV_REFLECT_RESOURCE_FLAG_CBV:
                case SPV_REFLECT_RESOURCE_FLAG_SRV: image_res_access_info.access |= ResourceAccessBit::eRead; break;
                case SPV_REFLECT_RESOURCE_FLAG_UAV: image_res_access_info.access |= ResourceAccessBit::eWrite; break;
            default: break;
            }
        }
        if (binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        || binding.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER ) {
            switch (binding.resource_type) {
                case SPV_REFLECT_RESOURCE_FLAG_CBV:
                case SPV_REFLECT_RESOURCE_FLAG_SRV: buffer_res_access_info.access |= ResourceAccessBit::eRead; break;
                case SPV_REFLECT_RESOURCE_FLAG_UAV: buffer_res_access_info.access |= ResourceAccessBit::eWrite; break;
            default: break;
            }
        }
    }

    static vk::PipelineStageFlagBits ShaderStageToPipelineStage(vk::ShaderStageFlagBits shader_stage) {
        switch (shader_stage) {
            case vk::ShaderStageFlagBits::eVertex: return vk::PipelineStageFlagBits::eVertexShader;
            case vk::ShaderStageFlagBits::eTessellationControl: return vk::PipelineStageFlagBits::eTessellationControlShader;
            case vk::ShaderStageFlagBits::eTessellationEvaluation: return vk::PipelineStageFlagBits::eTessellationEvaluationShader;
            case vk::ShaderStageFlagBits::eGeometry: return vk::PipelineStageFlagBits::eGeometryShader;
            case vk::ShaderStageFlagBits::eFragment: return vk::PipelineStageFlagBits::eFragmentShader;
            case vk::ShaderStageFlagBits::eCompute: return vk::PipelineStageFlagBits::eComputeShader;
        default: break;
        }
        return {};
    }

    Shader::Shader(Vulkan::Context& ctx, const std::filesystem::path& path) : DnmGLLite::Shader(ctx, path) {
        if (std::filesystem::exists(path))
            VulkanContext->Message(std::format("file not exists, {}", std::filesystem::absolute(path).string()), 
                MessageType::eInvalidBehavior);

        const auto shaderCode = LoadShaderFile(m_path);

        //create shader module
        m_shader_module = VulkanContext->GetDevice().createShaderModule(
            vk::ShaderModuleCreateInfo{}.setCode(shaderCode));
        
        //create shader reflection
        spv_reflect::ShaderModule reflect(shaderCode);
        m_stage = static_cast<vk::ShaderStageFlagBits>(reflect.GetShaderStage());
        m_buffer_resource_access_info.stages = ShaderStageToPipelineStage(m_stage);
        m_image_resource_access_info.stages = ShaderStageToPipelineStage(m_stage);

        {
            uint32_t count;
            reflect.EnumerateDescriptorSets(&count, nullptr);
            std::vector<SpvReflectDescriptorSet*> dst_sets(count);
            reflect.EnumerateDescriptorSets(&count, dst_sets.data());

            for (const auto* set : dst_sets) {
                auto& set_info = m_descriptor_sets.emplace_back();
                set_info.idx = set->set;

                for (auto i : Counter(set->binding_count)) {
                    const auto* binding = set->bindings[i];
                    const auto resource_type = static_cast<vk::DescriptorType>(binding->descriptor_type);

                    set_info.bindings.emplace_back(
                        binding->binding,
                        binding->count,
                        static_cast<vk::DescriptorType>(binding->descriptor_type)
                    );

                    FillResAccessInfo(*binding, m_buffer_resource_access_info, m_image_resource_access_info);
                }
            }
        }

        //push constants
        {
            uint32_t count;
            reflect.EnumeratePushConstantBlocks(&count, nullptr);
        
            std::vector<SpvReflectBlockVariable*> pushConstants(count);
            reflect.EnumeratePushConstantBlocks(&count, pushConstants.data());

            if (count != 0)
                m_push_constant = PushConstant {
                pushConstants[0]->size,
                pushConstants[0]->offset,
                };
        }
    }
}