cmake_minimum_required(VERSION 3.10)

project (BlitzenEngine VERSION 0)

#Require c++17 for VkBootstrap
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Vulkan REQUIRED COMPONENTS glslc)

add_library(BlitzenEngine 
        src/mainEngine.cpp
        src/mainEngine.h
        src/Inputs/glfwCallbacks.cpp
        src/Inputs/glfwCallbacks.h
        src/BlitzenVulkan/vulkanRenderer.cpp
        src/BlitzenVulkan/vulkanRenderer.h
        src/BlitzenVulkan/VkBootstrap.cpp
        src/BlitzenVulkan/vulkanSDKobjects.cpp
        src/BlitzenVulkan/vulkanSDKobjects.h
        src/BlitzenVulkan/vulkanPipelines.cpp
        src/BlitzenVulkan/vulkanPipelines.h
        src/BlitzenVulkan/vulkanRenderData.h
        src/BlitzenVulkan/vulkanRenderData.cpp
        src/AssetLoading/assetLoading.cpp
        src/AssetLoading/assetLoading.h
        ExternalDependencies/fastgltf/src/fastgltf.cpp
        ExternalDependencies/fastgltf/src/base64.cpp
        ExternalDependencies/fastgltf/src/simdjson.cpp)

add_subdirectory(ExternalDependencies/glfw ExternalDependencies/fastgltf)

target_link_directories(BlitzenEngine PUBLIC
                        "${PROJECT_SOURCE_DIR}/ExternalDependencies/Vulkan/Lib")

target_link_libraries(BlitzenEngine PUBLIC 
                        glfw 
                        vulkan-1)

target_include_directories(BlitzenEngine PUBLIC
                            "${PROJECT_SOURCE_DIR}/src"
                            "${PROJECT_SOURCE_DIR}/ExternalDependencies/GLFW/include"
                            "${PROJECT_SOURCE_DIR}/ExternalDependencies/Vulkan/include"
                            "${PROJECT_SOURCE_DIR}/ExternalDependencies/VkBootstrap"
                            "${PROJECT_SOURCE_DIR}/ExternalDependencies/fastgltf/include")


#Copy assets folder
add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/Assets ${CMAKE_CURRENT_BINARY_DIR}/Assets
)
add_dependencies(BlitzenEngine copy_assets)


#---------------------------------------------------------------------------------------------------
#Compiling Shaders
#

find_program(GLSL_VALIDATOR glslangValidator HINTS $"{PROJECT_SOURCE_DIR}/ExternalDependencies/Vulkan/Bin")

set(GLSL_VALIDATOR "${PROJECT_SOURCE_DIR}/ExternalDependencies/Vulkan/Bin/glslangValidator.exe")
  
  
  file(GLOB_RECURSE GLSL_SOURCE_FILES
      "VulkanShaders/*.frag"
      "VulkanShaders/*.vert"
      )
  
  foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/VulkanShaders/${FILE_NAME}.spv")
    add_custom_command(
      OUTPUT ${SPIRV}
      COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/VulkanShaders/"
      COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
      DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
  endforeach(GLSL)
  
  add_custom_target(
      VulkanShaders 
      DEPENDS ${SPIRV_BINARY_FILES}
      )
  
  add_dependencies(BlitzenEngine VulkanShaders)
  
  add_custom_command(TARGET BlitzenEngine POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:BlitzenEngine>/VulkanShaders/"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${PROJECT_BINARY_DIR}/VulkanShaders"
          "$<TARGET_FILE_DIR:BlitzenEngine>/VulkanShaders"
          )