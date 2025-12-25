cmake_minimum_required(VERSION 3.26)

project("DnmGLLite")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(Examples_ ON CACHE BOOL "examples")

file(GLOB Vulkan_Sources
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Vulkan/*.cpp
)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)

add_library(DnmGLLiteRHI_Vulkan SHARED ${Vulkan_Sources})

target_link_libraries(DnmGLLiteRHI_Vulkan PUBLIC
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    unofficial::spirv-reflect
) 

target_include_directories(DnmGLLiteRHI_Vulkan PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Header/
)

set_target_properties(DnmGLLiteRHI_Vulkan PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Vulkan
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Vulkan
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Vulkan
)

if(Examples_)
    add_subdirectory(Examples)
endif()
