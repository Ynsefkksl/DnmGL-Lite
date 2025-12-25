#pragma once

#include "DnmGLLite/Vulkan/Context.hpp"

namespace DnmGLLite::Vulkan {
    class Buffer final : public DnmGLLite::Buffer {
    public:
        Buffer(Vulkan::Context& context, const DnmGLLite::BufferDesc& desc);
        ~Buffer();

        [[nodiscard]] auto GetBuffer() const { return m_buffer; }
        [[nodiscard]] auto* GetAllocation() const { return m_allocation; }
    private:
        vk::Buffer m_buffer;
        VmaAllocation m_allocation;
    };
}