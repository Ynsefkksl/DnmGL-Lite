#pragma once

#include "DnmGLLite/DnmGLLite.hpp"
#include "DnmGLLite/Utility/Macros.hpp"

#ifdef OS_WIN
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #define VK_USE_PLATFORM_WIN32_KHR
    
    #include <vulkan/vulkan.hpp>

    #undef UpdateResource
#else
    #include <vulkan/vulkan.hpp>
#endif

#include <functional>
#include <vector>
#include <cstdint>

typedef struct VmaAllocator_T* VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;
typedef struct VmaPool_T* VmaPool;

#define VulkanContext reinterpret_cast<Vulkan::Context*>(context)
#define DECLARE_VK_FUNC(func_name) PFN_##func_name func_name

namespace DnmGLLite::Vulkan {
    // maybe this will be useful in the future
    enum class ContextState {
        eNone,
        eCommandBufferRecording,
        eCommandExecuting
    };

    enum class ResourceTypeBit : uint8_t {
        eBuffer = 0x1,
        eImage = 0x2,
    };
    using ResourceTypeFlag = Flags<ResourceTypeBit>;

    enum class ResourceAccessBit : uint8_t {
        eRead = 0x1,
        eWrite = 0x2,
    };
    using ResourceAccessFlag = Flags<ResourceAccessBit>;

    struct ResourceAccessInfo {
        ResourceTypeFlag type;
        ResourceAccessFlag access;
        vk::PipelineStageFlags stages;

        ResourceAccessInfo operator|(ResourceAccessInfo other) const noexcept {
            return {
                type | other.type,
                access | other.access,
                stages | other.stages,
            };
        }

        ResourceAccessInfo operator|=(ResourceAccessInfo other) const noexcept {
            return *this | other;
        }

        vk::AccessFlags GetVkAccessFlags() const {
            vk::AccessFlags out{};
            if (stages & (vk::PipelineStageFlagBits::eVertexShader |
                        vk::PipelineStageFlagBits::eFragmentShader |
                        vk::PipelineStageFlagBits::eTessellationControlShader |
                        vk::PipelineStageFlagBits::eTessellationEvaluationShader |
                        vk::PipelineStageFlagBits::eGeometryShader |
                        vk::PipelineStageFlagBits::eComputeShader)) {
                if (access.Has(ResourceAccessBit::eWrite))
                    out |= vk::AccessFlagBits::eShaderWrite;
                if (access.Has(ResourceAccessBit::eRead))
                    out |= vk::AccessFlagBits::eShaderRead;
            }
            if (stages & vk::PipelineStageFlagBits::eTransfer) {
                if (access.Has(ResourceAccessBit::eWrite))
                    out |= vk::AccessFlagBits::eTransferWrite;
                if (access.Has(ResourceAccessBit::eRead))
                    out |= vk::AccessFlagBits::eTransferRead;
            }
            return out;
        }
    };

    class CommandBuffer;
    class Shader;
    class Buffer;
    class Image;
    class Sampler;
    class RenderPass;

    // images must be this layout except for copy or transfer commands  
    inline vk::ImageLayout GetIdealImageLayout(DnmGLLite::ImageUsageFlags flags) {
        if (flags == DnmGLLite::ImageUsageBits::eSampled)
            return vk::ImageLayout::eShaderReadOnlyOptimal;

        else if (flags == DnmGLLite::ImageUsageBits::eColorAttachment
        || flags == (DnmGLLite::ImageUsageBits::eColorAttachment | DnmGLLite::ImageUsageBits::eTransientAttachment))
            return vk::ImageLayout::eColorAttachmentOptimal;

        else if (flags == DnmGLLite::ImageUsageBits::eDepthStencilAttachment 
        || flags == (DnmGLLite::ImageUsageBits::eDepthStencilAttachment | DnmGLLite::ImageUsageBits::eTransientAttachment))
            return vk::ImageLayout::eDepthStencilAttachmentOptimal;

        return vk::ImageLayout::eGeneral;
    }

    struct SwapchainProperties {
        vk::Format format;
        vk::ColorSpaceKHR color_space;
        vk::PresentModeKHR present_mode;
        vk::Extent2D extent;
        uint32_t image_count;
    
        operator std::string() {
            return std::format(
                "\nSwapchain Properties \nformat: {} \ncolor space: {} \npresent mode: {} \nextent: {}, {} \nimage count: {}\n", 
                                        vk::to_string(format),
                                        vk::to_string(color_space),
                                        vk::to_string(present_mode),
                                        extent.width, extent.height,
                                        image_count);
        }
    };

    [[nodiscard]] FORCE_INLINE inline vk::Format ToVk(Format v) {
        return static_cast<vk::Format>(v);
    }
    
    [[nodiscard]] FORCE_INLINE inline vk::AttachmentLoadOp ToVk(AttachmentLoadOp v) {
        return static_cast<vk::AttachmentLoadOp>(v);
    }

    [[nodiscard]] FORCE_INLINE inline vk::AttachmentStoreOp ToVk(AttachmentStoreOp v) {
        return static_cast<vk::AttachmentStoreOp>(v);
    }

    class Context final : public DnmGLLite::Context {
    public:
        struct InternalImageLayoutTranslation {
            vk::Image image;
            vk::ImageAspectFlags aspect;
            vk::ImageLayout layout;
        };

        struct InternalBufferResource {
            vk::Buffer buffer;
            vk::DescriptorType type;
            uint32_t offset;
            uint32_t size;

            vk::DescriptorSet set;
            uint32_t binding;
            uint32_t array_element;
        };

        struct InternalImageResource {
            vk::ImageView image_view;

            vk::DescriptorSet set;
            uint32_t binding;
            uint32_t array_element;
        };

        struct InternalTextureResource {
            vk::ImageView image_view;
            vk::ImageLayout image_layout;
            vk::Sampler sampler;

            vk::DescriptorSet set;
            uint32_t binding;
            uint32_t array_element;
        };

        struct SupportedFeatures {
            bool uniform_buffer_update_after_bind : 1 = false;
            bool storage_buffer_update_after_bind : 1 = false;
            bool storage_image_update_after_bind : 1 = false;
            bool sampled_image_update_after_bind : 1 = false;

            bool memory_budget : 1 = false;
            bool pageable_device_local_memory : 1 = false;
            bool memory_priority : 1 = false;
            bool sync2 : 1 = false;
            bool anisotropy : 1 = false;

            //chatgpt
            operator std::string() {
                std::string s("\nSupported Features: \n");
                s += "uniform_buffer_update_after_bind: " + std::string(uniform_buffer_update_after_bind ? "true" : "false") + "\n";
                s += "storage_buffer_update_after_bind: " + std::string(storage_buffer_update_after_bind ? "true" : "false") + "\n";
                s += "storage_image_update_after_bind: " + std::string(storage_image_update_after_bind ? "true" : "false") + "\n";
                s += "sampled_image_update_after_bind: " + std::string(sampled_image_update_after_bind ? "true" : "false") + "\n";
                s += "memory_budget: " + std::string(memory_budget ? "true" : "false") + "\n";
                s += "pageable_device_local_memory: " + std::string(pageable_device_local_memory ? "true" : "false") + "\n";
                s += "memory_priority: " + std::string(memory_priority ? "true" : "false") + "\n";
                s += "sync2: " + std::string(sync2 ? "true" : "false") + "\n";
                s += "anisotropy: " + std::string(anisotropy ? "true" : "false") + "\n";
                s += "\n";
                return s;
            }
        };
        
        struct DeviceFeatures {
            vk::QueueFlags queue_flags;
            uint32_t timestamp_valid_bits;
            uint32_t queue_family;
        };
    public:
        Context();
        ~Context();

        [[nodiscard]] constexpr GraphicsBackend GetGraphicsBackend() const noexcept override {
            return GraphicsBackend::eVulkan;
        }

        void Init(const ContextDesc&) override;

        void ExecuteCommands(const std::function<bool(DnmGLLite::CommandBuffer*)>& func) override;
        void Render(const std::function<bool(DnmGLLite::CommandBuffer*)>& func) override;
        void WaitForGPU() override;
        void Vsync(bool v) override { ReCreateSwapchain({m_swapchain_properties.extent.width, m_swapchain_properties.extent.height}, v); }
        
        [[nodiscard]] std::unique_ptr<DnmGLLite::Buffer> CreateBuffer(const DnmGLLite::BufferDesc&) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGLLite::Image> CreateImage(const DnmGLLite::ImageDesc&) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGLLite::Sampler> CreateSampler(const DnmGLLite::SamplerDesc&) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGLLite::Shader> CreateShader(const std::filesystem::path&) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGLLite::ResourceManager> CreateResourceManager(std::span<const DnmGLLite::Shader*>) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGLLite::ComputePipeline> CreateComputePipeline(const DnmGLLite::ComputePipelineDesc&) noexcept override;
        [[nodiscard]] std::unique_ptr<DnmGLLite::GraphicsPipeline> CreateGraphicsPipeline(const DnmGLLite::GraphicsPipelineDesc&) noexcept override;

        [[nodiscard]] auto GetInstance() const { return m_instance; }
        [[nodiscard]] auto GetSurface() const { return m_surface; }
        [[nodiscard]] auto GetDevice() const { return m_device; }
        [[nodiscard]] auto GetPhysicalDevice() const { return m_physical_device; }
        [[nodiscard]] auto GetQueue() const { return m_queue; }
        [[nodiscard]] auto GetSwapchain() const { return m_swapchain; }
        [[nodiscard]] auto GetCommandPool() const { return m_command_pool; }
        [[nodiscard]] auto GetDescriptorPool() const { return m_descriptor_pool; }
        [[nodiscard]] const auto& GetSwapchainImages() const { return m_swapchain_images; }
        [[nodiscard]] const auto& GetSwapchainImageViews() const { return m_swapchain_image_views; }
        [[nodiscard]] auto* GetVmaAllocator() const { return m_vma_allocator; }
        [[nodiscard]] auto GetSwapchainProperties() const { return m_swapchain_properties; }
        [[nodiscard]] auto GetImageIndex() const { return m_image_index; }
        [[nodiscard]] auto GetEmptySetLayout() const { return m_empty_set_layout; }
        [[nodiscard]] auto GetEmptySet() const { return m_empty_set; }
        [[nodiscard]] const auto& GetDispatcher() const { return dispatcher; }
    
        void ReCreateSwapchain(Uint2 extent, bool Vsync) { CreateSwapchain(extent, Vsync); }
    
        [[nodiscard]]auto GetSupportedFeatures() const { return supported_features; }
        [[nodiscard]]auto GetDeviceFeatures() const { return device_features; }

        void DeleteObject(const std::function<void(vk::Device device, VmaAllocator allocator)>& delete_func) {
            defer_vulkan_obj_delete.emplace_back(std::move(delete_func));
        }

        ContextState GetContextState();
        Vulkan::CommandBuffer* GetCommandBufferIfRecording();
        //just for new created images
        void DeferImageLayoutTransfer(const InternalImageLayoutTranslation& res);

        void DeferResourceUpdate(const std::span<const InternalBufferResource>& res);
        void DeferResourceUpdate(const std::span<const InternalImageResource>& res);
        void DeferResourceUpdate(const std::span<const InternalTextureResource>& res);

        void ProcessResource(
            const Context::InternalBufferResource& res, vk::DescriptorBufferInfo& info, vk::WriteDescriptorSet& write);
        void ProcessResource(
            const Context::InternalImageResource& res, vk::DescriptorImageInfo& info, vk::WriteDescriptorSet& write);
        void ProcessResource(
            const Context::InternalTextureResource& res, vk::DescriptorImageInfo& info, vk::WriteDescriptorSet& write);
    private:
        std::vector<std::function<void(vk::Device device, VmaAllocator allocator)>> defer_vulkan_obj_delete;

        using InternalResource = std::variant<InternalBufferResource, InternalImageResource, InternalTextureResource>;
        //just for new created images
        std::vector<InternalImageLayoutTranslation> defer_image_layout_transfer_array;
        std::vector<InternalResource> defer_resource_update;
        void ProcressImageLayoutTransfer();
        void ProcessResourceUpdates();
        void DeleteVulkanObjects();

        struct Dispatcher {
            constexpr uint32_t getVkHeaderVersion() const { return VK_HEADER_VERSION; }
            DECLARE_VK_FUNC(vkCreateDebugUtilsMessengerEXT);
            DECLARE_VK_FUNC(vkDestroyDebugUtilsMessengerEXT);
            DECLARE_VK_FUNC(vkCmdPipelineBarrier2KHR);
        } dispatcher;

        SupportedFeatures supported_features;
        DeviceFeatures device_features;

        void CreateInstance(WindowType window_type);
        void CreateDebugMessenger();
        void CreateSurface(const WindowHandle&);
        void CreateDevice();
        void CreateCommandPool();
        void CreateDescriptorPool();
        void CreateSwapchain(Uint2 extent, bool Vsync);
        void CreateVmaAllocator();
        void CreatePlaceholders();
        
        vk::Instance m_instance = VK_NULL_HANDLE;
        vk::DebugUtilsMessengerEXT m_debug_messanger = VK_NULL_HANDLE;
        vk::SurfaceKHR m_surface = VK_NULL_HANDLE;
        vk::PhysicalDevice m_physical_device = VK_NULL_HANDLE;
        vk::Device m_device = VK_NULL_HANDLE;
        vk::Queue m_queue = VK_NULL_HANDLE;
        vk::Fence m_fence = VK_NULL_HANDLE;
        vk::Semaphore m_acquire_next_image_semaphore = VK_NULL_HANDLE;
        vk::Semaphore m_render_finished_semaphore = VK_NULL_HANDLE;
        vk::CommandPool m_command_pool = VK_NULL_HANDLE;
        uint32_t m_image_index{};
        CommandBuffer* m_command_buffer;
        SwapchainProperties m_swapchain_properties;
        vk::SwapchainKHR m_swapchain = VK_NULL_HANDLE;
        std::vector<vk::Image> m_swapchain_images{};
        std::vector<vk::ImageView> m_swapchain_image_views{};
        VmaAllocator m_vma_allocator = VK_NULL_HANDLE;
        vk::DescriptorPool m_descriptor_pool;
        vk::DescriptorSetLayout m_empty_set_layout;
        vk::DescriptorSet m_empty_set;
        ContextState context_state = ContextState::eNone;
    };

    inline void Context::WaitForGPU() {
        [[maybe_unused]] auto _ = m_device.waitForFences(m_fence, vk::True, 1'000'000'000);
        m_device.resetFences(m_fence);
    }

    inline ContextState Context::GetContextState() {
        if (context_state == ContextState::eCommandExecuting) {
            if (m_device.getFenceStatus(m_fence) == vk::Result::eSuccess)
                context_state = ContextState::eNone;
        }
        return context_state;
    }

    inline Vulkan::CommandBuffer* Context::GetCommandBufferIfRecording() {
        if (context_state == ContextState::eCommandBufferRecording) {
            return m_command_buffer;
        }
        return nullptr;
    }

    inline void Context::ProcessResource(const Context::InternalBufferResource& res, vk::DescriptorBufferInfo& info, vk::WriteDescriptorSet& write) {
        info.setBuffer(res.buffer)
                .setOffset(res.offset)
                .setRange(res.size);

        write.setBufferInfo({info})
                .setDescriptorCount(1)
                .setDstArrayElement(res.array_element)
                .setDescriptorType(res.type)
                .setDstBinding(res.binding)
                .setDstSet(res.set);
    }

    inline void Context::ProcessResource(const Context::InternalImageResource& res, vk::DescriptorImageInfo& info, vk::WriteDescriptorSet& write) {
        info.setImageView(res.image_view)
            .setImageLayout(vk::ImageLayout::eGeneral);

        write.setImageInfo({info})
                .setDescriptorCount(1)
                .setDstArrayElement(res.array_element)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDstBinding(res.binding)
                .setDstSet(res.set);
    }

    inline void Context::ProcessResource(const Context::InternalTextureResource& res, vk::DescriptorImageInfo& info, vk::WriteDescriptorSet& write) {
        info.setImageView(res.image_view)
            .setImageLayout(res.image_layout)
            .setSampler(res.sampler)
            ;

        write.setImageInfo({info})
            .setDescriptorCount(1)
            .setDstArrayElement(res.array_element)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDstBinding(res.binding)
            .setDstSet(res.set);
    }

    inline void Context::DeferImageLayoutTransfer(const InternalImageLayoutTranslation& res) { 
        defer_image_layout_transfer_array.emplace_back(res);
    }

    inline void Context::DeferResourceUpdate(const std::span<const InternalBufferResource>& res) {
        defer_resource_update.reserve(res.size());
        for (const auto& r : res) {
            defer_resource_update.emplace_back(r);
        }
    }

    inline void Context::DeferResourceUpdate(const std::span<const InternalImageResource>& res) {
        defer_resource_update.reserve(res.size());
        for (const auto& r : res) {
            defer_resource_update.emplace_back(r);
        }
    }

    inline void Context::DeferResourceUpdate(const std::span<const InternalTextureResource>& res) {
        defer_resource_update.reserve(res.size());
        for (const auto& r : res) {
            defer_resource_update.emplace_back(r);
        }
    }

    inline void Context::DeleteVulkanObjects() {
        for (const auto& delete_func : defer_vulkan_obj_delete) {
            delete_func(m_device, m_vma_allocator);
        }
        defer_vulkan_obj_delete.resize(0);
    }
}

template <>
struct FlagTraits<DnmGLLite::Vulkan::ResourceTypeBit> {
    static constexpr bool isBitmask = true;
};

template <>
struct FlagTraits<DnmGLLite::Vulkan::ResourceAccessBit> {
    static constexpr bool isBitmask = true;
};
