cmake_minimum_required(VERSION 3.26)

project("DnmGLLite")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

option(Examples ON)

file(GLOB Vulkan_Sources
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Vulkan/*.cpp
)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)

add_library(DnmGLLite_Vulkan SHARED ${Vulkan_Sources})

target_link_libraries(DnmGLLite_Vulkan PUBLIC
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    unofficial::spirv-reflect
)

target_include_directories(DnmGLLite_Vulkan PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Header/
)

set_target_properties(DnmGLLite_Vulkan PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Vulkan
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Vulkan
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Vulkan
)

if(Examples)
    add_subdirectory(Examples)
endif()