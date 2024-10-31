#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vma/vk_mem_alloc.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"

#include <string>
#include <unordered_map>

namespace BlitzenRendering
{
    //This is the way the data that will be passed to each vertex is structured
    struct VulkanVertex
    {
        glm::vec3 position;
        float uv_x;
        glm::vec3 normal;
        float uv_y;
        glm::vec4 color;
    };

    /*-------------------------------------------------------------------------------------------------------
    Images outside the ones that were already created on the swapchain, will be allocated with the allocator.
    The AllocatedImage struct will hold information about the image itself and its allocation
    ---------------------------------------------------------------------------------------------------------*/
    struct VulkanAllocatedImage
    {
        VkImage image{VK_NULL_HANDLE};
        VkImageView imageView{VK_NULL_HANDLE};
        VkExtent3D extent{0, 0, 1};
        VkFormat format{VK_FORMAT_UNDEFINED};
        VmaAllocation allocation;

        void CleanupResources(const VkDevice& device, const VmaAllocator& allocator);
    };

    /*--------------------------------------------------------------------------------------------------------
    Buffers will be allocated with vma's allocator and will hold the allocation and allocation info as well
    ---------------------------------------------------------------------------------------------------------*/
    struct VulkanAllocatedBuffer
    {
        VkBuffer buffer{VK_NULL_HANDLE};
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo{};

        void CleanupResources(const VkDevice& device, const VmaAllocator& allocator);
    };

    /*-------------------------------------------------------------------------------------
    Vulkan will draw a mesh by passing its vertex and index buffer to the GPU as an SSBO 
    and getting the address of the vertex buffer to give to a push constant
    ----------------------------------------------------------------------------------------*/
    struct VulkanGPUMeshBuffers
    {
        VulkanAllocatedBuffer vertexBuffer;
        VulkanAllocatedBuffer indexBuffer; 
        VkDeviceAddress vertexBufferAddress; 

        void CleanupResources(const VkDevice& device, const VmaAllocator& allocator);
    };

    //This will be updated for each dynamic object to holds its vertex buffer address and its matrix
    struct GPUPushConstant
    {
        glm::mat4 worldMatrix;
        VkDeviceAddress vertexBuffer;
    };

    //Used to store all the different pipelines used by the different materials of the engine objects
    struct MaterialPipeline
    {
        VkPipeline graphicsPipeline;
        VkPipelineLayout pipelineLayout;
    };

    enum class MaterialPass : uint8_t
    {
        MP_transparentMaterial,
        MP_opaqueMaterial,
        default
    };

    //A specific instance of a material, holds the pipeline and descriptor sets to be bound
    struct MaterialInstance
    {
        MaterialPipeline* pPipeline;
        VkDescriptorSet descriptorSet;
        MaterialPass pass;
    };

    //Every surface will have its own draw call and uses these to draw indexed
    struct GeoSurface
    {
        uint32_t indexCount;
        uint32_t firstIndex;

        MaterialInstance* pMaterial;
    };

    //Holds a loaded mesh asset and the mesh buffers need to draw it
    struct VulkanMeshAsset
    {
        VulkanGPUMeshBuffers meshBuffers;
        std::vector<GeoSurface> geoSurfaces;

        std::string meshName;
    };

    //Holds scene data that does not change per object but is global
    struct GPUSceneData
    {
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::vec4 ambientColor;
        glm::vec4 sunlightColor;
        glm::vec4 sunlightDirection;
    };

    struct MaterialConstants 
    {
        //How lighting should affect normal textures
	    glm::vec4 colorFactors;
        //How lighting should affect textures with special metallic properties
	    glm::vec4 metal_rough_factors;
	    //padding, as this will be a uniform buffer
	    glm::vec4 extra[14];
	};

    struct MaterialResources 
    {
        //Holds the normal color part of the texture
	    VulkanAllocatedImage colorImage;
        //Sampler for the above texture
	    VkSampler colorSampler;
        //Holds the parts of the texture with special metallic properties like shininess
	    VulkanAllocatedImage metalRoughImage;
        //Sampler for the above texture
	    VkSampler metalRoughSampler;
	    VkBuffer dataBuffer;
	    uint32_t dataBufferOffset;
	};

    struct MaterialData 
    {
	    MaterialPipeline opaquePipeline;
	    MaterialPipeline transparentPipeline;

	    VkDescriptorSetLayout materialLayout;

        MaterialConstants materialConstants;

        MaterialResources materialResources;

	    void CleanupResources(const VkDevice& device);
    };


    //Holds everything needed for a surface to be drawn by vkCmdDrawIndexed
    struct VulkanRenderObject
    {
        //The starting index of the index buffer
        uint32_t firstIndex;
        uint32_t indexCount;

        //The index buffer to be bound
        VkBuffer* pIndexBuffer{nullptr};

        //The material used by the surface, will be used to bind the pipeline and the descriptor sets
        MaterialInstance* pMaterial{nullptr};

        //The model matrix of the render object, to be given to the push constant
        glm::mat4 transform{1.0f};
        //The address of the vertex buffer that holds the vertices of the surface
        VkDeviceAddress vertexBufferAddress;
    };

    struct DrawContext
    {
        std::vector<VulkanRenderObject> opaqueObjects;
    };

    class IRenderable
    {
        virtual void AddToDrawContext(const glm::mat4& topMatrix, DrawContext& drawContext) = 0;
    };

    enum class NodeType : uint8_t
    {
        NT_MeshNode,
        NT_Undefined
    };

    class Node : public IRenderable
    {
    public:
        Node* pParentNode;
        std::vector<Node> m_children;

        glm::mat4 localTransform;
        glm::mat4 worldTransform;

        //Used to identify how the node should be added to the draw context
        NodeType type = NodeType::NT_Undefined;

        VulkanMeshAsset* m_asset;

        void UpdateTransform(const glm::mat4& parentMatrix);

        virtual void AddToDrawContext(const glm::mat4& topMatrix, DrawContext& drawContext) override;    
    };

    class MeshNode : public Node
    {
    public:
        VulkanMeshAsset* m_asset;

        void AddToDrawContext(const glm::mat4& topMatrix, DrawContext& drawContext) override;
    };
}