#pragma once

#include "DnmGLLite.hpp"

namespace DnmGLLite {
    struct alignas(16) SpriteCameraData {
        Mat4x4 proj_mtx;
        Float3 pos;
    };

    struct alignas(16) SpriteData {
        ColorFloat color = {1,1,1,1};
        Float2 uv_up_right{};
        Float2 uv_bottom_left{};
        Float2 position = {0, 0};
        Float2 scale = {0.1, 0.1};
        float angle = {0};
        float color_factor = {0};
    
        FORCE_INLINE void SetColor(const ColorFloat& c) { color = c; }
        FORCE_INLINE const ColorFloat& GetColor() const { return color; }
    
        FORCE_INLINE void SetSpriteUv(const Float4& v) { uv_up_right.x = v.x; uv_up_right.y = v.y; uv_bottom_left.x = v.z; uv_bottom_left.y = v.w; }
    
        FORCE_INLINE void SetSpriteUpRight(const Float2& v) { uv_up_right = v; }
        FORCE_INLINE Float2 GetSpriteUpRight() const { return uv_up_right; }
    
        FORCE_INLINE void SetSpriteBottomLeft(const Float2& v) { uv_bottom_left = v; }
        FORCE_INLINE Float2 GetSpriteBottomLeft() const { return uv_bottom_left; }
    
        FORCE_INLINE void SetPosition(const Float2& p) { position = p; }
        FORCE_INLINE Float2 GetPosition() const { return position; }
        FORCE_INLINE void AddPosition(const Float2& d) { position += d; }
    
        FORCE_INLINE void SetScale(const Float2& s) { scale = s; }
        FORCE_INLINE Float2 GetScale() const { return scale; }
        FORCE_INLINE void AddScale(const Float2& d) { scale += d; }
    
        FORCE_INLINE void SetAngle(const float a) { angle = a; }
        FORCE_INLINE float GetAngle() const { return angle; }
        FORCE_INLINE void AddAngle(const float d) { angle += d; }
    
        FORCE_INLINE void SetColorFactor(const float f) { color_factor = f; }
        FORCE_INLINE float GetColorFactor() const { return color_factor; }
        FORCE_INLINE void AddColorFactor(const float d) { color_factor += d; }
    };

    class SpriteCamera {
    public:
        // ortho
        SpriteCamera(Float2 camera_extent, float near_, float far_) 
        : m_camera_extent(camera_extent), m_near(near_), m_far(far_), m_perspective(false) {}
        // perspective
        SpriteCamera(Float2 camera_extent, float fov, float near_, float far_) 
        : m_camera_extent(camera_extent), m_fov(fov), m_near(near_), m_far(far_), m_perspective(true) {}

        void SetProjection(float fov, float near_, float far_) {
            m_fov = fov;
            m_near = near_;
            m_far = far_;
            m_perspective = true;
        }

        void SetOrtho(float near_, float far_) {
            m_near = near_;
            m_far = far_;
            m_perspective = false;
        }
        
        Float2 m_camera_extent;
        float m_fov;
        float m_near;
        float m_far;

        SpriteCameraData& CalculateProjMtx() {
            if (m_perspective) {
                m_camera_data.proj_mtx = Perspective(m_fov, m_camera_extent.x / m_camera_extent.y, m_near, m_far);
            }
            else {
                m_camera_data.proj_mtx = Ortho(-m_camera_extent.x, m_camera_extent.x, -m_camera_extent.y, m_camera_extent.y, 0.01f, 1.f);
            }
            return m_camera_data;
        }

        SpriteCameraData& GetCameraData() {
            return m_camera_data;
        }

        void SetPos(Float3 pos) {
            m_camera_data.pos = pos;
        }

        Float3 GetPos() const {
            return m_camera_data.pos;
        }
    private:
        SpriteCameraData m_camera_data{};
        bool m_perspective{};
    };

    //pointers life cycle is managed by SpriteManager
    class SpriteHandle {
    public:
        SpriteHandle() = default;
        bool operator==(const SpriteHandle& other) const {
            return GetValue() == other.GetValue();
        }
    private:
        explicit SpriteHandle(uint32_t value) : value(new uint32_t(value)) {}

        void SetValue(const uint32_t newValue) const {
            *value = newValue;
        }

        [[nodiscard]] uint32_t GetValue() const {
            return *value;
        }

        void Invalidate() {
            delete value;
            value = nullptr;
        }

        bool isNull() const {
            return value == nullptr;
        }

        uint32_t *value{}; // point to spriteBuffer element
        friend class SpriteManager;
    };

    struct SpriteManagerDesc {
        Context *context;
        TextureResource* atlas_texture;
        Uint2 extent;
        SampleCount msaa;
        uint32_t init_capacity = 1024*64;
    };
    
    class SpriteManager {
    public:
        SpriteManager(const DnmGLLite::SpriteManagerDesc& desc);
        ~SpriteManager() noexcept {
            for (auto& handle : m_handles) {
                if (handle.isNull())
                    continue;
                handle.Invalidate();
            }
        }

        [[nodiscard]] auto GetSpriteCount() const { return m_sprite_count; }
        [[nodiscard]] auto GetCapacity() const { return m_sprite_buffer->GetDesc().size; }

        [[nodiscard]] auto* GetSpriteBuffer() const { return m_sprite_buffer.get(); }
        [[nodiscard]] auto* GetGraphicsPipeline() const  { return m_graphics_pipeline.get(); }
        [[nodiscard]] auto* GetResourceManager() const  { return m_resource_manager.get(); }
        [[nodiscard]] auto* GetVertexShader() const { return m_vertex_shader.get(); }
        [[nodiscard]] auto* GetFragmentShader() const { return m_fragment_shader.get(); }
        [[nodiscard]] auto* GetSpriteBufferMappedPtr() const noexcept { return m_sprite_buffer->GetMappedPtr<SpriteData>(); }
        [[nodiscard]] auto* GetContext() const { return m_graphics_pipeline->context; }
        [[nodiscard]] auto* GetCamera() const { return m_camera_ptr; }
        void SetCamera(SpriteCamera *camera) { m_camera_ptr = camera; }

        void RenderSprites(DnmGLLite::CommandBuffer* command_buffer, const DnmGLLite::ColorFloat &clear_color) noexcept;

        void ReserveSprite(DnmGLLite::CommandBuffer* command_buffer, uint32_t reserve_count) noexcept;

        std::optional<SpriteHandle> CreateSprite(const DnmGLLite::SpriteData& sprite_data) noexcept;
        std::vector<SpriteHandle> CreateSprites(std::span<const DnmGLLite::SpriteData> sprite_data) noexcept;
        SpriteHandle CreateSprite(DnmGLLite::CommandBuffer* command_buffer, const DnmGLLite::SpriteData& sprite_data) noexcept;
        std::vector<SpriteHandle> CreateSprites(DnmGLLite::CommandBuffer* command_buffer, std::span<const DnmGLLite::SpriteData> sprite_data) noexcept;

        void DeleteSprite(DnmGLLite::SpriteHandle& handle) noexcept;

        void SetSprite(DnmGLLite::SpriteHandle handle, const DnmGLLite::SpriteData& sprite_data) noexcept;
        void SetSprite(DnmGLLite::SpriteHandle handle, const auto& data, auto SpriteData::*member) noexcept;

        SpriteData GetSprite(DnmGLLite::SpriteHandle handle) noexcept;
    private:
        std::vector<SpriteHandle> CreateSpriteBase(std::span<const DnmGLLite::SpriteData> sprite_data);
        SpriteHandle CreateSpriteBase(const DnmGLLite::SpriteData& sprite_data);

        std::vector<DnmGLLite::SpriteHandle> m_handles{};

        DnmGLLite::GraphicsPipeline::Ptr m_graphics_pipeline{};
        DnmGLLite::ResourceManager::Ptr m_resource_manager{};
        DnmGLLite::Shader::Ptr m_vertex_shader{};
        DnmGLLite::Shader::Ptr m_fragment_shader{};

        DnmGLLite::Buffer::Ptr m_sprite_buffer{};
        SpriteCamera* m_camera_ptr{};
        uint32_t m_sprite_count{};
    };

    inline SpriteManager::SpriteManager(const DnmGLLite::SpriteManagerDesc& desc) {
        {
            const auto init_capacity = std::max(desc.init_capacity, 1u);
    
            m_sprite_buffer = desc.context->CreateBuffer({
                .size = init_capacity * sizeof(SpriteData),
                .memory_host_access = DnmGLLite::MemoryHostAccess::eWrite,
                .memory_type = DnmGLLite::MemoryType::eDeviceMemory,
                .buffer_flags = DnmGLLite::BufferUsageBits::eStorage,
            });
            
            m_handles.reserve(init_capacity);
        }

        {
            m_vertex_shader = desc.context->CreateShader("./Shaders/Bin/Sprite.vert.spv");
            m_fragment_shader = desc.context->CreateShader("./Shaders/Bin/Sprite.frag.spv");

            const DnmGLLite::Shader* shaders[2] = {m_vertex_shader.get(), m_fragment_shader.get()};
            m_resource_manager = desc.context->CreateResourceManager(
                shaders
            );

            m_graphics_pipeline = desc.context->CreateGraphicsPipeline({
                .vertex_shader = m_vertex_shader.get(),
                .fragment_shader = m_fragment_shader.get(),
                .resource_manager = m_resource_manager.get(),
                .color_load_op  = {DnmGLLite::AttachmentLoadOp::eClear},
                .color_store_op  = {DnmGLLite::AttachmentStoreOp::eStore},
                .depth_format = DnmGLLite::Format::eD16Norm,
                .depth_load_op = DnmGLLite::AttachmentLoadOp::eDontCare,
                .depth_store_op = DnmGLLite::AttachmentStoreOp::eDontCare,
                .cull_mode = DnmGLLite::CullMode::eNone,
                .topology = DnmGLLite::PrimitiveTopology::eTriangleStrip,
                .msaa = desc.msaa,
                .depth_test = false,
                .depth_write = true,
                .presenting = true, 
                .color_blend = true,
            });

            m_graphics_pipeline->SetAttachments({}, DnmGLLite::RenderAttachment{}, desc.extent);
        }

        {

            if (desc.atlas_texture) {
                desc.atlas_texture->binding = 1;
                desc.atlas_texture->set = 0;
                desc.atlas_texture->array_element = 0;
                m_resource_manager->SetResourceAsTexture(std::span(desc.atlas_texture, 1));    
            }
            else {
                const TextureResource atlas_tex_resource {
                    .image = GetContext()->GetPlaceholderImage(),
                    .sampler = GetContext()->GetPlaceholderSampler(),
                    .subresource = {},
                    .set = 0,
                    .binding = 1,
                    .array_element = 0,
                };
                m_resource_manager->SetResourceAsTexture({&atlas_tex_resource, 1});
            }
        }

        {
            const BufferResource sprite_buffer_resources[] = {
                {
                    .buffer = m_sprite_buffer.get(),
                    .type = BufferResourceType::eStorageBuffer,
                    .size = m_sprite_buffer->GetDesc().size,
                    .offset = 0,
                    .set = 0,
                    .binding = 0,
                    .array_element = 0,
                },
            };
            m_resource_manager->SetResourceAsBuffer(sprite_buffer_resources);
        }
    }

    inline void SpriteManager::ReserveSprite(DnmGLLite::CommandBuffer* command_buffer, uint32_t reserve_count) noexcept {
        if (GetCapacity() - GetSpriteCount() >= reserve_count
        || command_buffer == nullptr)
            return;

        m_handles.reserve(reserve_count);
        
        const auto new_buffer_size = 
            (GetCapacity() + (reserve_count < GetCapacity() ? GetCapacity() : reserve_count)) * sizeof(SpriteData);

        {
            auto new_buffer = GetContext()->CreateBuffer({
                .size = new_buffer_size,
                .memory_host_access = DnmGLLite::MemoryHostAccess::eWrite,
                .memory_type = DnmGLLite::MemoryType::eDeviceMemory,
                .buffer_flags = DnmGLLite::BufferUsageBits::eStorage,
            });

            command_buffer->CopyBufferToBuffer({
                .src_buffer = m_sprite_buffer.get(),
                .dst_buffer = new_buffer.get(),
                .copy_size = GetSpriteCount() * sizeof(SpriteData),
            });

            m_sprite_buffer.swap(new_buffer);

            const BufferResource res[] = {
                {
                    .buffer = m_sprite_buffer.get(),
                    .type = BufferResourceType::eStorageBuffer,
                    .size = m_sprite_buffer->GetDesc().size,
                    .offset = 0,
                    .set = 0,
                    .binding = 1,
                    .array_element = 0,
                },
            };
            m_resource_manager->SetResourceAsBuffer(res);
        }
    }

    inline std::optional<SpriteHandle> SpriteManager::CreateSprite(const DnmGLLite::SpriteData& sprite_data) noexcept {
        if (GetCapacity() - GetSpriteCount() < 1)
            return {};

        return CreateSpriteBase(sprite_data);
    }

    inline std::vector<SpriteHandle> SpriteManager::CreateSprites(std::span<const DnmGLLite::SpriteData> sprite_data) noexcept {
        const auto element_count = (GetCapacity() - GetSpriteCount()) - sprite_data.size();
        if (element_count == 0)
            return {};

        return CreateSpriteBase(std::span(sprite_data.data(), element_count));
    }

    inline SpriteHandle SpriteManager::CreateSprite(DnmGLLite::CommandBuffer* command_buffer, const DnmGLLite::SpriteData& sprite_data) noexcept {
        ReserveSprite(command_buffer, 1);

        return CreateSpriteBase(sprite_data);
    }

    inline std::vector<SpriteHandle> SpriteManager::CreateSprites(DnmGLLite::CommandBuffer* command_buffer, std::span<const DnmGLLite::SpriteData> sprite_data) noexcept {
        if (sprite_data.size() == 0)
            return {};

        ReserveSprite(command_buffer, sprite_data.size());

        return CreateSpriteBase(sprite_data);
    }

    inline SpriteHandle SpriteManager::CreateSpriteBase(const DnmGLLite::SpriteData& sprite_data) {
        memcpy(
            &reinterpret_cast<SpriteData*>(GetSpriteBufferMappedPtr())[GetSpriteCount()],
            &sprite_data, 
            sizeof(SpriteData)
        );
        
        return m_handles.emplace_back(SpriteHandle(m_sprite_count++));
    }

    inline std::vector<SpriteHandle> SpriteManager::CreateSpriteBase(std::span<const DnmGLLite::SpriteData> sprite_data) {
        memcpy(
            &GetSpriteBufferMappedPtr()[GetSpriteCount()],
            sprite_data.data(), 
            sizeof(SpriteData) * sprite_data.size()
        );

        m_handles.reserve(sprite_data.size());

        std::vector<SpriteHandle> out_handles(sprite_data.size());
        for (auto& handle : out_handles) {
            handle = m_handles.emplace_back(SpriteHandle(GetSpriteCount()));
            ++m_sprite_count;
        }
        return out_handles;
    }

    inline void SpriteManager::DeleteSprite(SpriteHandle& handle) noexcept {
        if (handle.isNull()) {
            return;
        }

        if (handle == m_handles.back()) {
            --m_sprite_count;
            handle.Invalidate();
            m_handles.pop_back();
            return;
        }

        {
            const auto *end_ptr = GetSpriteBufferMappedPtr() + --m_sprite_count;
            auto *deleted_sprite = GetSpriteBufferMappedPtr() + handle.GetValue();

            memcpy(deleted_sprite, end_ptr, sizeof(SpriteData));
        }

        const auto handle_it = m_handles.begin() + handle.GetValue();
        *handle_it = std::move(m_handles.back());
        handle_it->SetValue(handle.GetValue());
        handle.Invalidate();
        m_handles.pop_back();
    }

    inline void SpriteManager::SetSprite(DnmGLLite::SpriteHandle handle, const auto& data, auto SpriteData::*member) noexcept {
        if (handle.isNull()) {
            return;
        }

        memcpy(
            &((*(GetSpriteBufferMappedPtr() + handle.GetValue())).*member),
            &data,
            sizeof(data)
        );
    }

    inline void SpriteManager::SetSprite(DnmGLLite::SpriteHandle handle, const DnmGLLite::SpriteData& sprite_data) noexcept {
        if (handle.isNull()) {
            return;
        }

        memcpy(
            GetSpriteBufferMappedPtr() + handle.GetValue(),
            &sprite_data,
            sizeof(SpriteData)
        );
    }

    inline SpriteData SpriteManager::GetSprite(DnmGLLite::SpriteHandle handle) noexcept {
        return *(GetSpriteBufferMappedPtr() + handle.GetValue());   
    }

    inline void SpriteManager::RenderSprites(DnmGLLite::CommandBuffer* command_buffer, const DnmGLLite::ColorFloat &clear_color) noexcept {
        DnmGLLiteAssert(m_camera_ptr , "m_camera_ptr cannot be null");

        if (m_sprite_count) {
            command_buffer->BeginRendering(
                m_graphics_pipeline.get(), 
                std::span(&clear_color, 1), 
                DnmGLLite::DepthStencilClearValue{.depth = 0, .stencil = 0});

            command_buffer->PushConstant(
                m_graphics_pipeline.get(), 
                DnmGLLite::ShaderStageBits::eVertex, 
                0, 
                sizeof(SpriteCameraData), 
                &m_camera_ptr->GetCameraData());

            command_buffer->Draw(4, m_sprite_count);
            
                command_buffer->EndRendering(m_graphics_pipeline.get());
        }
    }
}
