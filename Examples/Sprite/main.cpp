#include "DnmGLLite/DnmGLLite.hpp"
#include "DnmGLLite/Loaders/Windows.hpp"
#include "DnmGLLite/Sprite.hpp"
#include "DnmGLLite/Utility/Container.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <exception>
#include <print>
#include <array>

constexpr DnmGLLite::Uint2 WindowExtent = {1280, 720};

constexpr DnmGLLite::Float2 spaceship_scale = {0.1, 0.1};
constexpr DnmGLLite::Float2 enemy_spaceship_scale = {0.1, 0.1};
constexpr DnmGLLite::Float2 bullet_scale = {0.03, 0.03};
constexpr DnmGLLite::Float2 enemy_bullet_scale = {0.03, 0.03};

constexpr DnmGLLite::Float4 spaceship_uv_coords = {0, 0.5, 0.49, 1};
constexpr DnmGLLite::Float4 enemy_spaceship_uv_coords = {0, 0.5, 0.49, 0.0};
constexpr DnmGLLite::Float4 bullet_uv_coords = {0.75, 1.0, 1.0, 0.75};
constexpr DnmGLLite::Float4 enemy_bullet_uv_coords = {0.5, 1.0, 0.75, 0.75};

constexpr float spaceship_speed = 0.005;
constexpr float enemy_speed = 0.002;
constexpr float bullet_speed = -0.002;
constexpr float enemy_bullet_speed = 0.002;

enum class ObjectType {
    eSpaceship = 0,
    eEnemySpaceship = 1,
    eBullet = 2,
    eEnemyBullet = 3,
};

constexpr std::array speeds {
    spaceship_speed,
    enemy_speed,
    bullet_speed,
    enemy_bullet_speed
};

constexpr std::array sprite_template {
    DnmGLLite::SpriteData {
        .color = {0,0,0,0},
        .uv_up_right = {spaceship_uv_coords.x, spaceship_uv_coords.y},
        .uv_bottom_left = {spaceship_uv_coords.z, spaceship_uv_coords.w},
        .position = {0, 0},
        .scale = spaceship_scale,
        .angle = 0,
        .color_factor = 0.f,
    },
    DnmGLLite::SpriteData {
        .color = {0,0,0,0},
        .uv_up_right = {enemy_spaceship_uv_coords.x, enemy_spaceship_uv_coords.y},
        .uv_bottom_left = {enemy_spaceship_uv_coords.z, enemy_spaceship_uv_coords.w},
        .position = {0, 0},
        .scale = enemy_spaceship_scale,
        .angle = 0,
        .color_factor = 0.f,
    },
    DnmGLLite::SpriteData {
        .color = {0,0,0,0},
        .uv_up_right = {bullet_uv_coords.x, bullet_uv_coords.y},
        .uv_bottom_left = {bullet_uv_coords.z, bullet_uv_coords.w},
        .position = {0, 0},
        .scale = bullet_scale,
        .angle = 0,
        .color_factor = 0.f,
    },
    DnmGLLite::SpriteData {
        .color = {0,0,0,0},
        .uv_up_right = {enemy_bullet_uv_coords.x, enemy_bullet_uv_coords.y},
        .uv_bottom_left = {enemy_bullet_uv_coords.z, enemy_bullet_uv_coords.w},
        .position = {0, 0},
        .scale = enemy_bullet_scale,
        .angle = 0,
        .color_factor = 0.f,
    },
};

static uint32_t createdObjectCount = 0;
static uint32_t updatedObjectCount = 0;
static uint32_t deletedObjectCount = 0;

static uint64_t totalCreatedObjectCount = 0;
static uint64_t totalUpdatedObjectCount = 0;
static uint64_t totalDeletedObjectCount = 0;

static DnmGLLite::SpriteManager *global_sprite_manager;

class Object {
public:
    Object(ObjectType obj_type, DnmGLLite::Float2 pos) 
    : m_pos(pos), m_obj_type(obj_type) {
        m_handle = global_sprite_manager->CreateSprite(sprite_template[int(obj_type)]).value();
        ++createdObjectCount;
    }

    void Delete() {
        global_sprite_manager->DeleteSprite(m_handle);
        ++deletedObjectCount;
    }

    void UpdatePos() {
        global_sprite_manager->SetSprite(m_handle, m_pos, &DnmGLLite::SpriteData::position);
        ++updatedObjectCount;
    }

    DnmGLLite::Float2 m_pos{};
    ObjectType m_obj_type;
    DnmGLLite::SpriteHandle m_handle;
    Container<Object>::Handle m_object_handle;
};

int main(int argc, char** args) {
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(WindowExtent.x, WindowExtent.y, "Triangle", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    try {
        DnmGLLite::Windows::ContextLoader ctx_loader("../Vulkan/DnmGLLiteRHI_Vulkan.dll");
        auto* context = ctx_loader.GetContext();

        context->Init({
                .window_extent = WindowExtent,
                .window_handle = DnmGLLite::WinWindowHandle {
                    .hwnd = glfwGetWin32Window(window),
                    .hInstance = GetModuleHandle(nullptr),
                },
                .Vsync = false,
            });

        DnmGLLite::Image::Ptr atlas_texture;

        context->ExecuteCommands([&] (DnmGLLite::CommandBuffer* command_buffer) -> bool {
            int x, y;
            auto *image_data = stbi_load("./Examples/Sprite/dnm.png", &x, &y, nullptr, 4);
            atlas_texture = context->CreateImage({
                .extent = {static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1},
                .format = DnmGLLite::Format::eRGBA8Norm,
                .usage_flags = DnmGLLite::ImageUsageBits::eSampled,
                .type = DnmGLLite::ImageType::e2D, 
                .mipmap_levels = 1,
                .sample_count = DnmGLLite::SampleCount::e1,
            });

            command_buffer->UploadData(atlas_texture.get(), DnmGLLite::ImageSubresource{}, image_data, x * y * 4, 0);
            stbi_image_free(image_data);
            return true;
        });

        auto texture_resource = DnmGLLite::TextureResource {
            .image = atlas_texture.get(),
            .sampler = context->GetPlaceholderSampler(),
            .subresource = {},
        };

        DnmGLLite::SpriteManager sprite_manager({
            .context = context,
            .atlas_texture = &texture_resource,
            .extent = WindowExtent,
            .msaa = DnmGLLite::SampleCount::e1,
            .init_capacity = 1024 * 512,
        });

        global_sprite_manager = &sprite_manager;

        {
            auto player = Object(ObjectType::eSpaceship, DnmGLLite::Float2{});
    
            Container<Object> bullets{};
            std::vector<Container<Object>::Handle> m_deleted_bullets{};
            Container<Object> enemys{};
            std::vector<Container<Object>::Handle> m_deleted_enemys{};
    
            uint32_t i{};
            while (!glfwWindowShouldClose(window)) {
                if (glfwGetKey(window, GLFW_KEY_ESCAPE))
                    break;

                if (i == 2000) {
                    std::println("created object count: {}\nupdated object count: {}\ndeletad object count: {}", 
                        createdObjectCount, updatedObjectCount, deletedObjectCount);
                    i = 0;
                }
                i++;

                createdObjectCount = 0; 
                updatedObjectCount = 0;
                deletedObjectCount = 0;

                {
                    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                        player.m_pos.y +=-spaceship_speed;
                    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                        player.m_pos.y += spaceship_speed;
                    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                        player.m_pos.x += spaceship_speed;
                    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                        player.m_pos.x +=-spaceship_speed;

                    player.UpdatePos();
                }

                if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                    auto handle = bullets.AddElement(Object(ObjectType::eBullet, player.m_pos - DnmGLLite::Float2{0, 0.1}));
                    bullets.GetElement(handle).m_object_handle = handle;
                }
                if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                    auto handle = enemys.AddElement(Object(ObjectType::eEnemySpaceship, DnmGLLite::Float2{0, -1}));
                    enemys.GetElement(handle).m_object_handle = handle;
                }
                if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
                    for (auto& enemy : enemys) {
                        auto handle = bullets.AddElement(Object(ObjectType::eEnemyBullet, enemy.m_pos + DnmGLLite::Float2{0, 0.1}));
                        bullets.GetElement(handle).m_object_handle = handle;
                    }
                }

                for (auto& object : enemys) {
                    object.m_pos.y += enemy_speed;
                    if (object.m_pos.y > 1) {
                        m_deleted_enemys.emplace_back(object.m_object_handle);
                    }
                    object.UpdatePos();
                }
                for (auto& object : bullets) {
                    object.m_pos.y += speeds[int(object.m_obj_type)];
                    if (object.m_obj_type == ObjectType::eBullet 
                    && object.m_pos.y < -1) {
                        m_deleted_bullets.emplace_back(object.m_object_handle);
                    }
                    else if (object.m_obj_type == ObjectType::eEnemyBullet 
                    && object.m_pos.y > 1) {
                        m_deleted_bullets.emplace_back(object.m_object_handle);
                    }
                    object.UpdatePos();
                }
    
                context->Render([&] (DnmGLLite::CommandBuffer* command_buffer) -> bool {
                    command_buffer->SetScissor({WindowExtent.x, WindowExtent.y}, {0, 0});
                    command_buffer->SetViewport({WindowExtent.x, WindowExtent.y}, {0, 0}, 0.f, 1.f);
    
                    sprite_manager.RenderSprites(command_buffer, {0, 0, 0, 0});
                    return true;
                });
    
                for (auto& handle : m_deleted_bullets) {
                    bullets.GetElement(handle).Delete();
                    bullets.DeleteElement(handle);
                }
                for (auto& handle : m_deleted_enemys) {
                    enemys.GetElement(handle).Delete();
                    enemys.DeleteElement(handle);
                }
                m_deleted_enemys.clear();
                m_deleted_bullets.clear();

                glfwPollEvents();
            }
        }
    }
    catch (const std::exception& e) {
        std::println("graphics error: {}", e.what());
    }

    glfwTerminate();
}

