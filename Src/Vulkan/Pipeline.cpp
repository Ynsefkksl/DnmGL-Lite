#include "DnmGLLite/Vulkan/Pipeline.hpp"
#include "DnmGLLite/Vulkan/Image.hpp"
#include "DnmGLLite/Vulkan/Shader.hpp"
#include "DnmGLLite/Vulkan/ResourceManager.hpp"

namespace DnmGLLite::Vulkan {
    static uint32_t GetFormatSize(Format format) {
        switch (format) {
            case Format::eR16UInt: 
            case Format::eR16Float: 
            case Format::eR16Norm: 
            case Format::eR16SNorm: 
            case Format::eR16SInt: return 2;
            case Format::eRG16UInt: 
            case Format::eRG16Float: 
            case Format::eRG16Norm: 
            case Format::eRG16SNorm: 
            case Format::eRG16SInt: return 4;
            case Format::eRGBA16UInt: 
            case Format::eRGBA16Float: 
            case Format::eRGBA16Norm: 
            case Format::eRGBA16SNorm: 
            case Format::eRGBA16SInt: return 8;
            case Format::eR32UInt:
            case Format::eR32Float:
            case Format::eR32SInt: return 4;
            case Format::eRG32UInt:
            case Format::eRG32Float:
            case Format::eRG32SInt: return 8;
            case Format::eRGB32UInt:
            case Format::eRGB32Float:
            case Format::eRGB32SInt: return 12;
            case Format::eRGBA32UInt:
            case Format::eRGBA32Float:
            case Format::eRGBA32SInt: return 16;
            default: return 0;
        }
    }

    static bool IsDepthFormat(Format format) {
        return (format == Format::eD16Norm) || (format == Format::eD32Float);
    }

    static bool IsStencilFormat(Format format) {
        return format == Format::eS8UInt;
    }

    static bool IsDepthStencilFormat(Format format) {
        return (format == Format::eD32NormS8UInt)
            || (format == Format::eD24NormS8UInt)
            || (format == Format::eD16NormS8UInt);
    }

    GraphicsPipeline::GraphicsPipeline(Vulkan::Context& ctx, const DnmGLLite::GraphicsPipelineDesc& desc) noexcept
        : DnmGLLite::GraphicsPipeline(ctx, desc) {

        DnmGLLiteAssert(m_desc.resource_manager, "resource manager can not be null")

        {
            bool vertex_shader_is_there{};
            bool fragment_shader_is_there{};

            for (const auto* shader : m_desc.resource_manager->GetShaders()) {
                vertex_shader_is_there |= m_desc.vertex_shader == shader;
                fragment_shader_is_there |= m_desc.fragment_shader == shader;
            }
            
            DnmGLLiteAssert(vertex_shader_is_there, "vertex shader must be in resource manager")
            DnmGLLiteAssert(fragment_shader_is_there, "fragment shader must be in resource manager")
        }

        if (IsDepthFormat(m_desc.depth_format)) {
            m_has_depth_attachment = true;
        }
        if (IsDepthStencilFormat(m_desc.depth_format)) {
            m_has_stencil_attachment = true;
            m_has_depth_attachment = true;
        }
        if (IsStencilFormat(m_desc.stencil_format)) {
            m_has_stencil_attachment = true;
        }

        CreateRenderpass();

        const auto *typed_vertex_shader = static_cast<const Vulkan::Shader *>(m_desc.vertex_shader);
        const auto *typed_fragment_shader = static_cast<const Vulkan::Shader *>(m_desc.fragment_shader);

        {
            m_buffer_resource_access_info |= typed_vertex_shader->GetBufferResourceAccessInfo();
            m_buffer_resource_access_info |= typed_fragment_shader->GetBufferResourceAccessInfo();
            m_image_resource_access_info |= typed_vertex_shader->GetImageResourceAccessInfo();
            m_image_resource_access_info |= typed_fragment_shader->GetImageResourceAccessInfo();
        }

        const auto *typed_resource_manager = static_cast<const Vulkan::ResourceManager *>(m_desc.resource_manager);
        const auto device = VulkanContext->GetDevice();

        {
            const Vulkan::Shader * shaders[] = {typed_vertex_shader, typed_fragment_shader};
            m_dst_sets = typed_resource_manager->GetDescriptorSets(shaders);

            std::vector<vk::PushConstantRange> push_constants{};

            if (typed_vertex_shader->GetPushConstants().has_value()) {
                push_constants.push_back(
                        vk::PushConstantRange{}.setOffset(typed_vertex_shader->GetPushConstants()->offset)
                                .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                                .setSize(typed_vertex_shader->GetPushConstants()->size)
                            );
            }

            if (typed_fragment_shader->GetPushConstants().has_value()) {
                push_constants.push_back(
                        vk::PushConstantRange{}.setOffset(typed_fragment_shader->GetPushConstants()->offset)
                                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                                .setSize(typed_fragment_shader->GetPushConstants()->size)
                            );
            }

            auto dst_set_layouts = typed_resource_manager->GetDescriptorLayouts(shaders);

            vk::PipelineLayoutCreateInfo create_info{};
            create_info.setSetLayouts(dst_set_layouts)
                        .setPushConstantRanges(push_constants)
                        ;

            m_pipeline_layout = device.createPipelineLayout(create_info);
        }

        const auto swapchain_properties = VulkanContext->GetSwapchainProperties();

        std::vector<vk::VertexInputAttributeDescription> vertex_attrib_desc{};
        vk::VertexInputBindingDescription vertex_binding_desc{};

        {
            uint32_t location{};
            uint32_t offset{};
            for (const auto& binding_format : desc.vertex_binding_formats) {
                const auto size = GetFormatSize(binding_format);
                DnmGLLiteAssert(size, "unsupported vertex binding format; location: {}", location)
                vertex_attrib_desc.emplace_back(
                    location++,
                    0,
                    ToVk(binding_format),
                    offset
                );
                offset += size;
            }

            vertex_binding_desc
                .setInputRate(vk::VertexInputRate::eVertex)
                .setBinding(0)
                .setStride(offset);
        }

        vk::PipelineVertexInputStateCreateInfo vertex_input_state_create_info{};
        vertex_input_state_create_info.setVertexAttributeDescriptions(vertex_attrib_desc)
                                    .setPVertexBindingDescriptions(&vertex_binding_desc)
                                    .setVertexBindingDescriptionCount(vertex_attrib_desc.size() ? 1 : 0)
                                    ;

        vk::PipelineInputAssemblyStateCreateInfo input_assembly_info{};
        input_assembly_info.setTopology(static_cast<vk::PrimitiveTopology>(m_desc.topology));
    
        vk::PipelineShaderStageCreateInfo shader_stage_create_info[2]{};
        shader_stage_create_info[0].setStage(vk::ShaderStageFlagBits::eVertex)
                                    .setModule(typed_vertex_shader->GetShaderModule())
                                    .setPName("main")
                                    ;

        shader_stage_create_info[1].setStage(vk::ShaderStageFlagBits::eFragment)
                                    .setModule(typed_fragment_shader->GetShaderModule())
                                    .setPName("main")
                                    ;

        vk::PipelineViewportStateCreateInfo viewport_state_create_info{};
        viewport_state_create_info
            .setScissorCount(1).setViewportCount(1);

        vk::PipelineRasterizationStateCreateInfo rester_info{};
        rester_info.setPolygonMode(static_cast<vk::PolygonMode>(m_desc.polygone_mode))
                    .setLineWidth(1.f)
                    .setCullMode(static_cast<vk::CullModeFlagBits>(m_desc.cull_mode))
                    .setFrontFace(static_cast<vk::FrontFace>(m_desc.front_face))
                    ;

        vk::PipelineMultisampleStateCreateInfo multisample_Info{};
        multisample_Info.setSampleShadingEnable(vk::False)
                        .setRasterizationSamples(static_cast<vk::SampleCountFlagBits>(m_desc.msaa))
                        .setMinSampleShading(1.f)
                        ;

        vk::PipelineDepthStencilStateCreateInfo depth_stencil_info{};
        depth_stencil_info.setDepthTestEnable(m_desc.depth_test)
                            .setDepthWriteEnable(m_desc.depth_write)
                            .setDepthCompareOp(static_cast<vk::CompareOp>(m_desc.depth_test_compare_op))
                            .setStencilTestEnable(m_desc.stencil_test)
                            ;

        std::vector<vk::PipelineColorBlendAttachmentState> blend_state{};
        for (auto i : Counter(
            m_desc.presenting ? 1 : m_desc.color_attachment_formats.size()))
                blend_state.emplace_back(vk::PipelineColorBlendAttachmentState{}
                    .setBlendEnable(m_desc.color_blend)
                    .setColorWriteMask(
                        vk::ColorComponentFlagBits::eA | 
                        vk::ColorComponentFlagBits::eR | 
                        vk::ColorComponentFlagBits::eB | 
                        vk::ColorComponentFlagBits::eG)
                    .setAlphaBlendOp(vk::BlendOp::eAdd)
                    .setColorBlendOp(vk::BlendOp::eAdd)
                    .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                    .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                    .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
                    .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                );

        vk::PipelineColorBlendStateCreateInfo color_blend_info{};
        color_blend_info.setLogicOpEnable(vk::False)
                        .setAttachments(blend_state);

        constexpr vk::DynamicState dynamic_states[2] {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{};
        dynamic_state_create_info.setDynamicStates(dynamic_states);

        vk::GraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.setLayout(m_pipeline_layout)
                    .setStages(shader_stage_create_info)
                    .setPViewportState(&viewport_state_create_info)
                    .setPRasterizationState(&rester_info)
                    .setPMultisampleState(&multisample_Info)
                    .setPDepthStencilState(&depth_stencil_info)
                    .setPColorBlendState(&color_blend_info)
                    .setPInputAssemblyState(&input_assembly_info)
                    .setPDynamicState(&dynamic_state_create_info)
                    .setPVertexInputState(&vertex_input_state_create_info)
                    .setRenderPass(m_renderpass)
                    .setSubpass(0)
                    ;

        m_pipeline = device.createGraphicsPipeline(nullptr, pipeline_info).value;
    }

    void GraphicsPipeline::CreateRenderpass() noexcept {
        const auto device = VulkanContext->GetDevice();

        std::vector<vk::AttachmentDescription> attachment_descs{};
        std::vector<vk::AttachmentReference> color_references{};
        std::vector<vk::AttachmentReference> resolve_references{};

        for (const auto i : Counter(GetColorAttachmentCount())) {
            attachment_descs.emplace_back(
                vk::AttachmentDescriptionFlags{},
                m_desc.presenting ? VulkanContext->GetSwapchainProperties().format : ToVk(m_desc.color_attachment_formats[i]),
                vk::SampleCountFlagBits::e1,
                ToVk(m_desc.color_load_op[i]),
                ToVk(m_desc.color_store_op[i]),
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                vk::ImageLayout::eUndefined,
                m_desc.presenting ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eColorAttachmentOptimal
            );

            if (HasMsaa()) {
                resolve_references.emplace_back(
                    i,
                    vk::ImageLayout::eColorAttachmentOptimal
                );
            }
            else {
                color_references.emplace_back(
                    i,
                    vk::ImageLayout::eColorAttachmentOptimal
                );
            }
        }

        if (HasMsaa())
            for (const auto i : Counter(GetColorAttachmentCount())) {
                attachment_descs.emplace_back(
                    vk::AttachmentDescriptionFlags{},
                    m_desc.presenting ? VulkanContext->GetSwapchainProperties().format : ToVk(m_desc.color_attachment_formats[i]),
                    static_cast<vk::SampleCountFlagBits>(m_desc.msaa),
                    vk::AttachmentLoadOp::eDontCare,
                    vk::AttachmentStoreOp::eDontCare,
                    vk::AttachmentLoadOp::eDontCare,
                    vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eColorAttachmentOptimal
                );

                color_references.emplace_back(
                    GetColorAttachmentCount() + i,
                    vk::ImageLayout::eColorAttachmentOptimal
                );
            }

        vk::AttachmentReference depth_stencil_reference{};
        depth_stencil_reference.setAttachment(color_references.size() * (HasMsaa() + 1))
                                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                                ;

        if (m_has_depth_attachment && m_has_stencil_attachment) {
            attachment_descs.emplace_back(
                vk::AttachmentDescriptionFlags{},
                ToVk(m_desc.depth_format), //or stencil_format is equal
                static_cast<vk::SampleCountFlagBits>(m_desc.msaa),
                ToVk(m_desc.depth_load_op),
                ToVk(m_desc.depth_store_op),
                ToVk(m_desc.stencil_load_op),
                ToVk(m_desc.stencil_store_op),
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthStencilAttachmentOptimal
            );
        }
        else if (m_has_depth_attachment) {
            attachment_descs.emplace_back(
                vk::AttachmentDescriptionFlags{},
                ToVk(m_desc.depth_format), //or stencil_format is equal
                static_cast<vk::SampleCountFlagBits>(m_desc.msaa),
                ToVk(m_desc.depth_load_op),
                ToVk(m_desc.depth_store_op),
                vk::AttachmentLoadOp::eDontCare,
                vk::AttachmentStoreOp::eDontCare,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthStencilAttachmentOptimal
            );
        }
        else if (m_has_stencil_attachment) {
            attachment_descs.emplace_back(
                vk::AttachmentDescriptionFlags{},
                ToVk(m_desc.stencil_format),
                vk::SampleCountFlagBits::e1,
                vk::AttachmentLoadOp{},
                vk::AttachmentStoreOp{},
                ToVk(m_desc.stencil_load_op),
                ToVk(m_desc.stencil_store_op),
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eDepthStencilAttachmentOptimal
            );
        }

        vk::SubpassDescription subpass_desc{};
        subpass_desc.setPDepthStencilAttachment(
                        (m_has_depth_attachment || m_has_stencil_attachment) 
                        ? &depth_stencil_reference : nullptr)
                    .setResolveAttachments(resolve_references)
                    .setColorAttachments(color_references)
                    ;

        vk::SubpassDependency subpass_dependency{};
        subpass_dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                            .setSrcAccessMask({})
                            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                            .setDstSubpass(0)
                            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
                            ;

        if (HasDepthAttachment() || HasStencilAttachment()) {
            subpass_dependency.dstAccessMask |= vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            subpass_dependency.dstStageMask |= vk::PipelineStageFlagBits::eEarlyFragmentTests;
            subpass_dependency.srcStageMask |= vk::PipelineStageFlagBits::eEarlyFragmentTests;
        }

        vk::RenderPassCreateInfo create_info{};
        create_info.setSubpasses({subpass_desc})
                    .setAttachments({attachment_descs})
                    .setDependencies({subpass_dependency});

        m_renderpass = device.createRenderPass(create_info, nullptr);
    }

    void GraphicsPipeline::SetAttachments(
            std::span<const DnmGLLite::RenderAttachment> color_attachments, 
            const std::optional<DnmGLLite::RenderAttachment> &depth_stencil_attachment,
            Uint2 extent) {
        DestroyFramebuffer();

        m_render_area = vk::Extent2D{extent.x, extent.y};
        
        m_user_color_attachments.resize(0);
        m_user_depth_stencil_attachment = nullptr;
        m_attachments.clear();
        
        std::vector<vk::ImageView> image_views{};
        image_views.reserve(color_attachments.size() + 2);

        if (m_desc.presenting) {
            image_views.emplace_back(VulkanContext->GetSwapchainImageViews()[0]);
        }
        else {
            DnmGLLiteAssert(color_attachments.size() == GetColorAttachmentCount(), "color_attachments.size() and GetColorAttachmentCount() must be same. except presenting")

            for (const auto& attachment : color_attachments) {
                DnmGLLiteAssert(attachment.image != UseInternalAttachmentResource, "just depth attachment can be null")

                auto* typed_image = reinterpret_cast<Vulkan::Image*>(attachment.image);
                image_views.emplace_back(typed_image->CreateGetImageView(attachment.subresource));
                m_user_color_attachments.emplace_back(typed_image);
            }
        }

        if (HasMsaa())
            for (auto i : Counter(GetColorAttachmentCount())) {
                image_views.emplace_back(CreateInternalResource(
                        extent, 
                        ImageUsageBits::eColorAttachment | ImageUsageBits::eTransientAttachment, 
                        m_desc.presenting ? static_cast<Format>(VulkanContext->GetSwapchainProperties().format) : m_desc.color_attachment_formats[i],
                        m_desc.msaa));
            }

        if (depth_stencil_attachment.has_value()) {
            const auto& attachment = depth_stencil_attachment.value();
            if (attachment.image == UseInternalAttachmentResource) {
                DnmGLLiteAssert(m_has_depth_attachment, "just depth attachment can be null")
                
                image_views.emplace_back(CreateInternalResource(
                    extent, 
                    ImageUsageBits::eDepthStencilAttachment | ImageUsageBits::eTransientAttachment, 
                    m_desc.depth_format, 
                    m_desc.msaa));
            }
            else {
                auto* typed_image = reinterpret_cast<Vulkan::Image*>(attachment.image);
                image_views.emplace_back(typed_image->CreateGetImageView(attachment.subresource));
                m_user_depth_stencil_attachment = typed_image;
            }
        }

        vk::FramebufferCreateInfo framebuffer_info{};
        framebuffer_info.setAttachments(image_views)
                        .setWidth(extent.x)
                        .setHeight(extent.y)
                        .setRenderPass(m_renderpass)
                        .setLayers(1)
                        ;

        if (m_desc.presenting) {
            for (const auto image : VulkanContext->GetSwapchainImageViews()) {
                image_views[0] = image;
                m_swapchain_framebuffers.emplace_back(VulkanContext->GetDevice().createFramebuffer(framebuffer_info));
            }
        }
        else {
            m_framebuffer = VulkanContext->GetDevice().createFramebuffer(framebuffer_info);
        }
    }

    vk::ImageView GraphicsPipeline::CreateInternalResource(Uint2 extent, ImageUsageFlags usage, Format format, SampleCount sample_count )noexcept {
        auto& image = m_attachments.emplace_back(std::make_unique<Vulkan::Image>(
            *VulkanContext, 
            DnmGLLite::ImageDesc{
                .extent = {extent.x, extent.y, 1},
                .format = format,
                .usage_flags = usage,
                .type = ImageType::e2D,
                .mipmap_levels = 1,
                .sample_count = sample_count,
            }));

        return image->CreateGetImageView({
            .type = ImageResourceType::e2D,
            .base_layer = 0,
            .base_mipmap = 0,
            .layer_count = 1,
            .mipmap_level = 1,
        });
    }

    ComputePipeline::ComputePipeline(Vulkan::Context& ctx, const DnmGLLite::ComputePipelineDesc& desc) noexcept
        : DnmGLLite::ComputePipeline(ctx, desc) {

        DnmGLLiteAssert(m_desc.resource_manager, "resource manager can not be null")
        
        {
            bool shader_is_there = false;

            for (const auto* shader : m_desc.resource_manager->GetShaders())
                shader_is_there |= m_desc.shader == shader;

            DnmGLLiteAssert(shader_is_there, "shader must be in resource manager")
        }

        const auto *typed_shader = static_cast<const Vulkan::Shader *>(m_desc.shader);

        {
            m_buffer_resource_access_info |= typed_shader->GetBufferResourceAccessInfo();
            m_image_resource_access_info |= typed_shader->GetImageResourceAccessInfo();
        }

        const auto *typed_resource_manager = static_cast<const Vulkan::ResourceManager *>(m_desc.resource_manager);
        const auto device = VulkanContext->GetDevice();

        {
            const Vulkan::Shader * shaders[] = {typed_shader};
            m_dst_sets = typed_resource_manager->GetDescriptorSets(shaders);
            auto dst_set_layouts = typed_resource_manager->GetDescriptorLayouts(shaders);

            vk::PushConstantRange push_constant;

            if (typed_shader->GetPushConstants().has_value()) {
                push_constant = vk::PushConstantRange{}.setStageFlags(vk::ShaderStageFlagBits::eCompute)
                                            .setOffset(typed_shader->GetPushConstants()->offset)
                                            .setSize(typed_shader->GetPushConstants()->size);
            }

            vk::PipelineLayoutCreateInfo create_info{};
            create_info.setSetLayouts(dst_set_layouts)
                        .setPushConstantRanges(
                            push_constant
                        )
                        .setPushConstantRangeCount(typed_shader->GetPushConstants().has_value());
            m_pipeline_layout = device.createPipelineLayout(create_info);
        }

        vk::PipelineShaderStageCreateInfo stage_info{};
        stage_info.setStage(vk::ShaderStageFlagBits::eCompute)
                    .setModule(typed_shader->GetShaderModule())
                    .setPName("name")
                    ;

        vk::ComputePipelineCreateInfo pipeline_info{};
        pipeline_info.setLayout(nullptr)
                    .setStage(stage_info)
                    .setLayout(m_pipeline_layout)
                    ;

        m_pipeline = device.createComputePipeline(nullptr, pipeline_info).value;
    }
}