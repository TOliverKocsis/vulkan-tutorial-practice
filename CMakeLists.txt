cmake_minimum_required(VERSION 3.29)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(VulkanTutorialPractice)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan 1.4.335 REQUIRED)

# Vulkan.hpp interface with required defines
add_library(VulkanCppModule INTERFACE)
target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
target_compile_definitions(VulkanCppModule INTERFACE
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)

# Find slangc for shader compilation
find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

# Compile shaders/slang.slang -> shaders/slang.spv at build time
add_custom_command(
    OUTPUT  ${CMAKE_BINARY_DIR}/shaders/slang.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
    COMMAND ${SLANGC_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/shaders/slang.slang
            -target spirv
            -profile spirv_1_4+spvRayQueryKHR
            -emit-spirv-directly
            -fvk-use-entrypoint-name
            -entry vertMain
            -entry fragMain
            -o ${CMAKE_BINARY_DIR}/shaders/slang.spv
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/slang.slang
    COMMENT "Compiling Slang shaders"
    VERBATIM
)
add_custom_target(slang_shader DEPENDS ${CMAKE_BINARY_DIR}/shaders/slang.spv)

add_executable(VulkanApp src/main.cpp)
set_target_properties(VulkanApp PROPERTIES CXX_STANDARD 20)
target_compile_definitions(VulkanApp PRIVATE
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
)
target_link_libraries(VulkanApp PRIVATE VulkanCppModule glfw glm::glm)
add_dependencies(VulkanApp slang_shader)
