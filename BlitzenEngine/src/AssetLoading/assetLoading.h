#include "fastgltf/parser.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "glm/gtx/quaternion.hpp"
#include "BlitzenVulkan/vulkanRenderer.h"

namespace BlitzenEngine
{
    //Loades an entire scene from a GLTF file
    void LoadScene(std::string_view filepath, BlitzenRendering::VulkanRenderer* pVulkan, 
    BlitzenRendering::LoadedGLTF& scene);
}
