#include "fastgltf/parser.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "BlitzenVulkan/vulkanRenderer.h"

namespace BlitzenEngine
{
    void LoadMeshAsset(std::filesystem::path filepath, BlitzenRendering::VulkanRenderer* pVulkan);
}
