#include "fastgltf/parser.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "BlitzenVulkan/vulkanRenderer.h"

namespace BlitzenEngine
{
	void LoadMeshAsset(std::filesystem::path filepath, std::vector<BlitzenRendering::VulkanVertex>& vertices,
		       	std::vector<uint32_t>&	indices,BlitzenRendering::VulkanRenderer* pVulkan);
}
