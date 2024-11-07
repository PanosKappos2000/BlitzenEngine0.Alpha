#include "fastgltf/parser.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "BlitzenVulkan/vulkanRenderer.h"
#include "glm/gtx/quaternion.hpp"

namespace BlitzenEngine
{
	void LoadScene(std::string_view filepath, std::vector<BlitzenRendering::VulkanVertex>& vertices,
	std::vector<uint32_t>&	indices, BlitzenRendering::LoadedGLTFScene& scene, 
	BlitzenRendering::VulkanRenderer* pVulkan);

	void LoadImage(BlitzenRendering::VulkanAllocatedImage& imageToLoad, BlitzenRendering::VulkanRenderer* pVulkan, 
	fastgltf::Asset& gltfAsset, fastgltf::Image& gltfImage);
}
