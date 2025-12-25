#pragma once

#include "DnmGLLite/Utility/Counter.hpp"
#include "DnmGLLite/Utility/Macros.hpp"
#include "DnmGLLite/Utility/Flag.hpp"
#include "DnmGLLite/Utility/Math.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <print>
#include <format>
#include <span>
#include <stdexcept>
#include <variant>
#include <filesystem>
#include <unordered_set>
#include <source_location>

namespace DnmGLLite {
    class Context;
    class CommandBuffer;
    class Buffer;
    class Image;
    class Shader;
    class Sampler;
    class ComputePipeline;
    class GraphicsPipeline;
    class ResourceManager;

    template <class... Types> 
    inline constexpr void DnmGLAssertFunc(std::string_view func_name, std::string_view condition_str, bool condition, const std::format_string<Types...> fmt, Types&&... args) {
        if (condition) {
            return;
        }
        std::println("[DnmGLLite Error ({})] Condition: {}, Message: {}", func_name, condition_str, std::vformat(fmt.get(), std::make_format_args(args...)));
        exit(1);
    }

    #define DnmGLLiteAssert(condition, fmt, ...) \
        DnmGLAssertFunc(std::source_location::current().function_name(), #condition, condition, fmt, __VA_ARGS__);

    //TODO: add compressed formats
    //equal to VkFormat
    enum class Format : uint8_t {
        eUndefined   = 0,
        eR8UInt      = 13,
        eR8Norm      = 9,
        eR8SNorm     = 10,
        eR8SInt      = 14,
        eRG8UInt     = 20,
        eRG8Norm     = 16,
        eRG8SNorm    = 17,
        eRG8SInt     = 21,
        eRGBA8UInt   = 41,
        eRGBA8Norm   = 37,
        eRGBA8SNorm  = 38,
        eRGBA8SInt   = 42,
        eRGBA8Srgb   = 43,
        eR16UInt     = 74,
        eR16Float    = 76,
        eR16Norm     = 70,
        eR16SNorm    = 71,
        eR16SInt     = 75,
        eRG16UInt    = 81,
        eRG16Float   = 83,
        eRG16Norm    = 77,
        eRG16SNorm   = 78,
        eRG16SInt    = 82,
        eRGBA16UInt  = 95,
        eRGBA16Float = 97,
        eRGBA16Norm  = 91,
        eRGBA16SNorm = 92,
        eRGBA16SInt  = 96,
        eR32UInt     = 98,
        eR32Float    = 100,
        eR32SInt     = 99,
        eRG32UInt    = 101,
        eRG32Float   = 103,
        eRG32SInt    = 102,
        eRGB32UInt   = 104,
        eRGB32Float  = 106,
        eRGB32SInt   = 105,
        eRGBA32UInt  = 107,
        eRGBA32Float = 109,
        eRGBA32SInt  = 108,
        eD16Norm    = 124,
        eD32Float   = 126,
        eD16NormS8UInt = 128,
        eD24NormS8UInt = 129,
        eD32NormS8UInt = 130,
        eS8UInt = 127,
    };
    
    enum class BufferResourceType {
        eUniformBuffer,
        eStorageBuffer
    };

    enum class ImageResourceType {
        e1D,
        e2D,
        e2DArray,
        e3D,
        eCube,
    };

    enum class ImageType : uint8_t {
        e1D,
        e2D,
        e3D,
    };

    enum class MemoryType : uint8_t {
        eAuto,  
        eHostMemory,
        eDeviceMemory,
    };

    enum class MemoryHostAccess : uint8_t {
        eNone,
        eReadWrite,
        eWrite
    };

    //same with vulkan
    enum class SamplerMipmapMode {
        eNearest,
        eLinear,
    };

    //same with vulkan
    enum class CompareOp {
        eNever,
        eLess,
        eEqual,
        eLessOrEqual,
        eGreater,
        eNotEqual,
        eGreaterOrEqual,
        eAlways
    };

    //same with vulkan
    enum class PolygonMode {
        eFill,
        eLine,
        ePoint
    };

    //same with vulkan
    enum class CullMode {
        eNone,
        eFront,
        eBack,
        eFrontAndBack,
    };

    //same with vulkan
    enum class FrontFace {
        eCounterClockwise,
        eClockwise
    };

    enum class SamplerFilter {
        //same with vulkan
        eNearest,
        eLinear,

        //not same with vulkan
        //if don't supported uses linear
        eAnisotropyX2 = 2,
        eAnisotropyX4 = 4,
        eAnisotropyX8 = 8,
        eAnisotropyX16 = 16,
    };

    //same with vulkan
    enum class AttachmentLoadOp {
        eLoad,
        eClear,
        eDontCare
    };

    //same with vulkan
    enum class AttachmentStoreOp {
        eStore,
        eDontCare
    };

    //same with vulkan
    enum class IndexType {
        eUint16,
        eUint32
    };

    //same with vulkan
    enum class PrimitiveTopology {
        ePointList,
        eLineList,
        eLineStrip,
        eTriangleList,
        eTriangleStrip,
        eTriangleFan,
        eLineListWithAdjacency,
        eLineStripWithAdjacency,
        eTriangleListWithAdjacency,
        eTriangleStripWithAdjacency,
        ePatchList
    };

    //same with vulkan
    enum class SamplerAddressMode {
        eRepeat,
        eMirroredRepeat,
        eClampToEdge,
        eClampToBorder
    };

    //same with vulkan
    enum class SampleCount {
        e1 = 1 << 0,
        e2 = 1 << 1,
        e4 = 1 << 2,
        e8 = 1 << 3,
        e16 = 1 << 4,
    };

    enum class BufferUsageBits : uint8_t {
        eVertex =   0x1,
        eIndex =    0x2,
        eUniform =  0x4,
        eStorage =  0x8,
        eIndirect = 0x10
    };
    using BufferUsageFlags = Flags<BufferUsageBits>;

    enum class ImageUsageBits : uint8_t {
        eSampled = 0x1,
        eStorage = 0x2,
        eColorAttachment = 0x4,
        eDepthStencilAttachment = 0x8,
        eTransientAttachment = 0x10
    };
    using ImageUsageFlags = Flags<ImageUsageBits>;

    //same with vulkan
    enum class ShaderStageBits : uint32_t {
        eVertex = 0x1,
        eTessellationControl = 0x2,
        eTessellationEvaluation = 0x4,
        eGeometry = 0x8,
        eFragment = 0x10,
        eCompute = 0x20,
        eAllGraphics = 0x1f,
        eAll = 0x7FFFFFFF,
    };
    using ShaderStageFlags = Flags<ShaderStageBits>;

    //same with vulkan
    enum class PipelineStageBits : uint32_t {
        eVertex = 0x1,
        eTessellationControl = 0x2,
        eTessellationEvaluation = 0x4,
        eGeometry = 0x8,
        eFragment = 0x10,
        eCompute = 0x20,
        eAllGraphics = 0x1f,
        eAll = 0x7FFFFFFF,
    };
    using PipelineStageFlags = Flags<PipelineStageBits>;

    struct Color { uint8_t r, g, b, a; };
    struct ColorFloat { float r, g, b, a; };

    struct DepthStencilClearValue { 
        float depth; 
        uint32_t stencil; 
    };

    struct ImageDesc {
        //if type 2D and z > 1, z is array count 
        Uint3 extent = {1, 1, 1};
        Format format;
        ImageUsageFlags usage_flags;
        ImageType type;
        uint32_t mipmap_levels = 1;
        SampleCount sample_count = SampleCount::e1;
    };

    struct ImageSubresource {
        ImageResourceType type = ImageResourceType::e2D;
        uint32_t base_layer = 0;
        uint32_t base_mipmap = 0;
        uint32_t layer_count = 1;
        uint32_t mipmap_level = 1;

        auto operator<=>(const ImageSubresource&) const = default;
    };

    constexpr DnmGLLite::Image *UseInternalAttachmentResource = nullptr;
    struct RenderAttachment {
        DnmGLLite::Image* image;
        ImageSubresource subresource;
    };

    struct RenderPassDesc {
        Format depth_stencil_format;
        AttachmentLoadOp depth_stencil_load_op;
        AttachmentStoreOp depth_stencil_store_op;
        std::vector<Format> color_formats;
        std::vector<AttachmentLoadOp> color_load_op;
        std::vector<AttachmentStoreOp> color_store_op;
    };

    struct SamplerDesc {
        SamplerMipmapMode mipmap_mode;
        CompareOp compare_op;
        SamplerFilter filter;
        SamplerAddressMode address_mode_u{};
        SamplerAddressMode address_mode_v{};
        SamplerAddressMode address_mode_w{};
    };

    struct BufferDesc {
        uint64_t size;
        MemoryHostAccess memory_host_access;
        MemoryType memory_type;
        BufferUsageFlags buffer_flags;
    };

    struct GpuMemoryDesc {
        BufferDesc buffer_desc;
        uint32_t aligment;
    };

    struct RenderPassBeginInfo {
        std::span<ColorFloat> color_clear_values;
        std::optional<DepthStencilClearValue> depth_stencil_clear_value;
    };

    struct BufferResource {
        DnmGLLite::Buffer* buffer;
        BufferResourceType type;
        uint64_t size;
        uint32_t offset;

        uint32_t set;
        uint32_t binding;
        uint32_t array_element;
    };

    struct ImageResource {
        DnmGLLite::Image* image;
        
        ImageSubresource subresource;

        uint32_t set;
        uint32_t binding;
        uint32_t array_element;
    };

    struct TextureResource {
        DnmGLLite::Image* image;
        DnmGLLite::Sampler* sampler;

        ImageSubresource subresource;

        uint32_t set;
        uint32_t binding;
        uint32_t array_element;
    };

    struct GraphicsPipelineDesc {
        Shader* vertex_shader;
        Shader* fragment_shader;
        ResourceManager* resource_manager;
        std::vector<Format> color_attachment_formats;
        std::vector<Format> vertex_binding_formats;
        std::vector<AttachmentLoadOp> color_load_op;
        std::vector<AttachmentStoreOp> color_store_op;
        // depth_format ignore if !(depth_test || depth_write) 
        Format depth_format;
        Format stencil_format;
        AttachmentLoadOp depth_load_op;
        AttachmentStoreOp depth_store_op;
        AttachmentLoadOp stencil_load_op;
        AttachmentStoreOp stencil_store_op;
        CompareOp depth_test_compare_op;
        PolygonMode polygone_mode;
        CullMode cull_mode;
        FrontFace front_face;
        PrimitiveTopology topology;
        //msaa none is SampleCount::e1
        SampleCount msaa = SampleCount::e1;
        // if depth_test false ignore
        // depth_load_op
        // depth_test_compare_op
        bool depth_test : 1;
        // if stencil_test false ignore
        // depth_store_op
        // depth_test_compare_op
        bool depth_write : 1;
        // if stencil_test false ignore
        // stencil_format
        // stencil_store_op, stencil_load_op
        bool stencil_test : 1;
        // if presenting true ignore
        // color_attachment_formats
        // color_load_op, color_store_op
        // and attachments
        // SetAttachments dosn't anything for color attachments, uses swapchain images
        bool presenting : 1;
        bool color_blend : 1;
    };

    struct ComputePipelineDesc {
        Shader* shader;
        ResourceManager* resource_manager;
    };

    struct BufferToBufferCopyDesc {
        const Buffer* src_buffer;
        const Buffer* dst_buffer;
        uint32_t src_offset;
        uint32_t dst_offset;
        uint64_t copy_size;
    };

    struct BufferToImageCopyDesc {
        const Buffer* src_buffer;
        Image* dst_image;
        ImageSubresource image_subresource;
        uint32_t buffer_offset;
        uint32_t buffer_row_lenght;
        uint32_t buffer_image_height;
        Uint3 image_offset;
        Uint3 image_extent;
    };

    struct ImageToImageCopyDesc {
        Image* src_image;
        Image* dst_image;
        ImageSubresource src_image_subresource;
        ImageSubresource dst_image_subresource;
        Uint3 src_offset;
        Uint3 dst_offset;
        Uint3 extent;
    };

    struct ImageToBufferCopyDesc {
        Image* src_image;
        const Buffer* dst_buffer;
        ImageSubresource image_subresource;
        uint32_t buffer_offset;
        uint32_t buffer_row_lenght;
        uint32_t buffer_image_height;
        Uint3 image_offset;
        Uint3 image_extent;
    };

    enum class MessageType {
        eInfo,
        eUnknown,
        eOutOfMemory,
        eInvalidBehavior,
        eUnsupportedDevice,
        eOutOfDateSwapchain,
        eInvalidState,
        eShaderCompilationFailed,
        eDeviceLost,
        eGraphicsBackendInternal,
    };

    enum class GraphicsBackend {
        eVulkan,
        eD3D12, // planned
        eMetal, // planned
    };

    enum class WindowType {
        eNone = 0,
        eWindows,
        eMac,
        eIos,
        eWayland,
        eAndroid,
        eX11,
    };

    struct WinWindowHandle {
        void* hwnd;
        void* hInstance;
    };

    struct MacWindowHandle {
        
    };

    struct IosWindowHandle {
        
    };

    struct AndroidWindowHandle {
        
    };

    struct WaylandWindowHandle {
        
    };

    struct X11WindowHandle {
        
    };

    using WindowHandle = std::variant<
                            std::nullopt_t,
                            WinWindowHandle, 
                            MacWindowHandle,
                            IosWindowHandle,
                            WaylandWindowHandle,
                            AndroidWindowHandle,
                            X11WindowHandle>;
                            
    inline WindowType GetWindowType(const WindowHandle& handle) {
        switch (handle.index()) {
            case 1: return WindowType::eWindows; break;
            case 2: return WindowType::eMac; break;
            case 3: return WindowType::eIos; break;
            case 4: return WindowType::eWayland; break;
            case 5: return WindowType::eAndroid; break;
            case 6: return WindowType::eX11; break;
        }
        return WindowType::eNone;
    }

    struct ContextDesc {
        Uint2 window_extent;
        WindowHandle window_handle;
        bool Vsync;
    };

    using CallbackFunc = std::function<void(std::string_view message, MessageType error, std::string_view source)>;

    class Context {
    public:
        virtual ~Context() = default;

        [[nodiscard]] virtual GraphicsBackend GetGraphicsBackend() const noexcept = 0;

        virtual void Init(const ContextDesc&) = 0;
        //offline rendering
        virtual void ExecuteCommands(const std::function<bool(CommandBuffer*)>& func) = 0;
        //ExecuteCommands + present image
        virtual void Render(const std::function<bool(CommandBuffer*)>& func) = 0;
        virtual void WaitForGPU() = 0;
        virtual void Vsync(bool) = 0;

        [[nodiscard]] virtual std::unique_ptr<DnmGLLite::Buffer> CreateBuffer(const DnmGLLite::BufferDesc&) noexcept = 0;
        [[nodiscard]] virtual std::unique_ptr<DnmGLLite::Image> CreateImage(const DnmGLLite::ImageDesc&) noexcept = 0;
        [[nodiscard]] virtual std::unique_ptr<DnmGLLite::Sampler> CreateSampler(const DnmGLLite::SamplerDesc&) noexcept = 0;
        [[nodiscard]] virtual std::unique_ptr<DnmGLLite::Shader> CreateShader(const std::filesystem::path&) noexcept = 0;
        [[nodiscard]] virtual std::unique_ptr<DnmGLLite::ResourceManager> CreateResourceManager(std::span<const DnmGLLite::Shader*>) noexcept = 0;
        [[nodiscard]] virtual std::unique_ptr<DnmGLLite::ComputePipeline> CreateComputePipeline(const DnmGLLite::ComputePipelineDesc&) noexcept = 0;
        [[nodiscard]] virtual std::unique_ptr<DnmGLLite::GraphicsPipeline> CreateGraphicsPipeline(const DnmGLLite::GraphicsPipelineDesc&) noexcept = 0;

        [[nodiscard]] constexpr DnmGLLite::Image* GetPlaceholderImage() const noexcept { return placeholder_image; };
        [[nodiscard]] constexpr DnmGLLite::Sampler* GetPlaceholderSampler() const noexcept { return placeholder_sampler; };
        constexpr void SetCallbackFunc(CallbackFunc& func) noexcept { callback_func.swap(func); };
        constexpr void Message(
            std::string_view message, 
            MessageType error,
            std::string_view source = std::source_location::current().function_name()) {
            callback_func(message, error, source);
        };
    protected:
        DnmGLLite::Image* placeholder_image;
        DnmGLLite::Sampler* placeholder_sampler;
        CallbackFunc callback_func;
    };

    class ContextLoader {
    public:
        ContextLoader() = default;
        virtual ~ContextLoader() = default;
        
        DnmGLLite::Context *GetContext() const { return context; }

        ContextLoader(ContextLoader&&) = delete;
        ContextLoader(ContextLoader&) = delete;
        ContextLoader& operator=(ContextLoader&&) = delete;
        ContextLoader& operator=(ContextLoader&) = delete;
    private:
        DnmGLLite::Context *context = nullptr;
    }; 

    class RHIObject {
    public:
        RHIObject(Context& context)
            : context(&context) {}

        RHIObject(RHIObject&&) = delete;
        RHIObject(RHIObject&) = delete;
        RHIObject& operator=(RHIObject&&) = delete;
        RHIObject& operator=(RHIObject&) = delete;

        Context* context;
    };

    class Buffer : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGLLite::Buffer>;
        Buffer(Context& context, const DnmGLLite::BufferDesc& desc) 
            : RHIObject(context),
            m_desc(desc) {}
        virtual ~Buffer() = default;

        template <typename T = uint8_t>
        [[nodiscard]] constexpr T* GetMappedPtr() const noexcept { return reinterpret_cast<T*>(m_mapped_ptr); }

        [[nodiscard]] constexpr const auto& GetDesc() const noexcept { return m_desc; }
    protected:
        uint8_t* m_mapped_ptr;

        DnmGLLite::BufferDesc m_desc;
    };

    class Image : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGLLite::Image>;
        Image(Context& context, const DnmGLLite::ImageDesc& desc)
            : RHIObject(context),
            m_desc(desc) {}
                     
        virtual ~Image() = default;

        [[nodiscard]] constexpr const auto& GetDesc() const noexcept { return m_desc; }
    protected:
        DnmGLLite::ImageDesc m_desc;
    };

    class Sampler : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGLLite::Sampler>;
        Sampler(Context& context, const DnmGLLite::SamplerDesc& desc)
            : RHIObject(context),
            m_desc(desc) {}
                     
        virtual ~Sampler() = default;

        [[nodiscard]] constexpr const auto& GetDesc() const noexcept { return m_desc; }
    protected:
        DnmGLLite::SamplerDesc m_desc;
    };

    class Shader : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGLLite::Shader>;
        Shader(Context& context, const std::filesystem::path& path) 
            : RHIObject(context), 
            m_path(path) {}

        virtual ~Shader() = default;

        [[nodiscard]] constexpr const auto& GetPath() const noexcept { return m_path; }
    protected:
        std::filesystem::path m_path;
    };

    class ResourceManager : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGLLite::ResourceManager>;
        ResourceManager(Context& context, std::span<const DnmGLLite::Shader*> shaders) 
            : RHIObject(context), 
            m_shaders(shaders.begin(), shaders.end()) {}
        virtual ~ResourceManager() = default;

        virtual void SetResourceAsBuffer(std::span<const BufferResource> update_resource) = 0;
        virtual void SetResourceAsImage(std::span<const ImageResource> update_resource) = 0;
        virtual void SetResourceAsTexture(std::span<const TextureResource> update_resource) = 0;

        [[nodiscard]] constexpr const auto& GetShaders() const noexcept { return m_shaders; }
    protected:
        const std::vector<const Shader*> m_shaders;
    };

    class GraphicsPipeline : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGLLite::GraphicsPipeline>;
        GraphicsPipeline(Context& context, const GraphicsPipelineDesc& desc)
            : RHIObject(context),
            m_desc(desc) {}

        virtual void SetAttachments(
            std::span<const DnmGLLite::RenderAttachment> attachments, 
            const std::optional<DnmGLLite::RenderAttachment> &depth_stencil_attachment, 
            Uint2 extent) = 0;

        virtual ~GraphicsPipeline() = default;

        [[nodiscard]] constexpr const auto& GetDesc() const noexcept { return m_desc; }
    protected:
        GraphicsPipelineDesc m_desc;
    };

    class ComputePipeline : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGLLite::ComputePipeline>;
        ComputePipeline(Context& context, const ComputePipelineDesc& desc) 
            : RHIObject(context),
            m_desc(desc) {}
        virtual ~ComputePipeline() = default;

         [[nodiscard]] constexpr const auto& GetDesc() const noexcept { return m_desc; }
    protected:
        ComputePipelineDesc m_desc;
    };

    class CommandBuffer : public RHIObject {
    public:
        using Ptr = std::unique_ptr<DnmGLLite::CommandBuffer>;
        CommandBuffer(Context& context) : RHIObject(context) {}
        virtual ~CommandBuffer() = default;

        virtual void Begin() = 0;
        virtual void End() = 0;

        virtual void BeginRendering(
            const DnmGLLite::GraphicsPipeline *pipeline,
            std::span<const DnmGLLite::ColorFloat> color_clear_values, 
            std::optional<DnmGLLite::DepthStencilClearValue> depth_stencil_clear_value) = 0;
        virtual void EndRendering(const DnmGLLite::GraphicsPipeline *pipeline) = 0;

        virtual void BindPipeline(const DnmGLLite::ComputePipeline* pipeline) = 0;

        virtual void Draw(uint32_t vertex_count, uint32_t instance_count) = 0;
        virtual void DrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset) = 0;

        virtual void SetViewport(Float2 extent, Float2 offset, float min_depth, float max_depth) = 0;
        virtual void SetScissor(Uint2 extent, Uint2 offset) = 0;

        virtual void PushConstant(const DnmGLLite::GraphicsPipeline* pipeline, DnmGLLite::ShaderStageFlags pipeline_stage, uint32_t offset, uint32_t size, const void *ptr) = 0;
        virtual void PushConstant(const DnmGLLite::ComputePipeline* pipeline, DnmGLLite::ShaderStageFlags pipeline_stage, uint32_t offset, uint32_t size, const void *ptr) = 0;

        virtual void CopyImageToBuffer(const DnmGLLite::ImageToBufferCopyDesc& desc) = 0;
        virtual void CopyImageToImage(const DnmGLLite::ImageToImageCopyDesc& descs) = 0;
        virtual void CopyBufferToImage(const DnmGLLite::BufferToImageCopyDesc& descs) = 0;
        virtual void CopyBufferToBuffer(const DnmGLLite::BufferToBufferCopyDesc& descs) = 0;
        virtual void UploadData(DnmGLLite::Image *image, const ImageSubresource& subresource, const void *data, uint32_t size, Uint3 offset) = 0;
        virtual void UploadData(const DnmGLLite::Buffer *buffer, const void* data, uint32_t size, uint32_t offset) = 0;
        virtual void GenerateMipmaps(DnmGLLite::Image *image) = 0;

        virtual void BindVertexBuffer(const DnmGLLite::Buffer *buffer, uint64_t offset) = 0;
        virtual void BindIndexBuffer(const DnmGLLite::Buffer *buffer, uint64_t offset, DnmGLLite::IndexType index_type) = 0;
        
        template <typename T> constexpr void UploadData(const DnmGLLite::Buffer *buffer, std::span<const T> data, uint32_t offset);
        template <typename T> constexpr void UploadData(DnmGLLite::Image *image, const ImageSubresource& subresource, std::span<const T> data, Uint3 offset);
    };

    template <typename T> 
    inline constexpr void CommandBuffer::UploadData(const DnmGLLite::Buffer *buffer, std::span<const T> data, uint32_t offset) {
        UploadData(buffer, data.data(), data.size() * sizeof(T), offset);
    }
    template <typename T> 
    inline constexpr void CommandBuffer::UploadData(DnmGLLite::Image *image, const ImageSubresource& subresource, std::span<const T> data, Uint3 offset) {
        UploadData(image, subresource, data.data(), data.size() * sizeof(T), offset);
    }
}

template <>
struct FlagTraits<DnmGLLite::BufferUsageBits> {
    static constexpr bool isBitmask = true;
};

template <>
struct FlagTraits<DnmGLLite::ImageUsageBits> {
    static constexpr bool isBitmask = true;
};

template <>
struct FlagTraits<DnmGLLite::ShaderStageBits> {
    static constexpr bool isBitmask = true;
};

template <>
struct FlagTraits<DnmGLLite::PipelineStageBits> {
    static constexpr bool isBitmask = true;
};
