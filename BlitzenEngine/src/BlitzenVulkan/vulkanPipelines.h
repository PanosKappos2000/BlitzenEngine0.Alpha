#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <vector>
#include <fstream>
#include <iostream>

#include "vulkanRenderData.h"

namespace BlitzenRendering
{
    #define VULKAN_OPAQUE_GEOMETRY_VERTEX_SHADER_FILENAME "BlitzenEngine/VulkanShaders/OpaqueGeometryShader.vert.spv"
    #define VULKAN_OPAQUE_GEOMETRY_FRAGMENT_SHADER_FILENAME "BlitzenEngine/VulkanShaders/OpaqueGeometryShader.frag.spv"

    class VulkanGraphicsPipelineBuilder
    {
    public:
        //At the start of this class, it will simply be give a reference to the device of the VulkanRenderer
        inline void Init(VkDevice* pDevice) {m_pDevice = pDevice;}

        //Asks the graphics pipeline builder to build the default pipeline used for opaque surfaces
        void BuildBasicOpaqueSurfacePipeline(VkPipeline* graphicsPipeline, 
        VkPipelineLayout* pipelineLayout, VkFormat* pColorAttachmentFormats, uint32_t colorAttachmentCount, 
        VkFormat pDepthAttachmentFormat, VkFormat pStencilAttachmentFormat);

        //Clears the data from the previous pipelines that were built and sets the pipeline and layout pointers
        void StartSettingUp(VkPipeline* pPipeline, VkPipelineLayout* pLayout, VkDevice* pDevice);

        //Loads the shader code into an array wraps it in a shader module and creates the shader stage
        void LoadShader(const char* filepath, std::vector<char>& code, VkShaderModule& module, 
        uint32_t stageIndex, VkShaderStageFlagBits shaderStage);

        void SetupDefaults();

        void SetPolygonMode(VkPolygonMode polygonMode, float lineWidth = 1.0f);

        void SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);

        void SetBasicTriangleTopology();

        void SetupDynamicViewport();

        void DisableMultisampling();

        void EnableDepthTest(VkCompareOp compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL);

        void SetDepthBoundsTest(VkBool32 bDepthBounds = VK_FALSE, float minDepthBounds = 0.f, 
	    float maxDepthBounds = 1.f);

        void DisableColorBlending();

        void SetupRenderingInfo(VkFormat* pColorAttachmentFormats, uint32_t colorAttachmentCount, 
        VkFormat pDepthAttachmentFormat, VkFormat pStencilAttachmentFormat);

        void SetupPipelineLayout(VkPushConstantRange* pPushConstants, uint32_t pushConstantCount, 
        VkDescriptorSetLayout* pDescriptorLayouts, uint32_t descriptorLayoutCount);

        //When one of the specialized build functions are done, this is called to actually create the pipeline
        void Build();



        VulkanGraphicsPipelineBuilder();
        VulkanGraphicsPipelineBuilder operator = (VulkanGraphicsPipelineBuilder& vulkan) = delete;
    private:

        //When a new build function is called, this makes sure that all member variable have been reset
        void Clear();

        //Read the data from a shader file in an array of char
        void ReadShaderFile(const char* filename, std::vector<char>& shaderCode);
    
    private:

        VkPipeline* m_pGraphicsPipeline;
        VkPipelineLayout* m_pPipelineLayout;
        
        /*--------------------------------------------------------------------------------------------------
        All variables needed to setup the graphics pipeline's states and initialize the create info sturct
        ----------------------------------------------------------------------------------------------------*/
        std::array<VkPipelineShaderStageCreateInfo, 2> m_shaderStages;
        VkPipelineVertexInputStateCreateInfo m_vertexInput{};
        VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{};
        VkPipelineTessellationStateCreateInfo m_tessellation{};
        VkPipelineViewportStateCreateInfo m_viewport{};
        VkPipelineRasterizationStateCreateInfo m_rasterization{};
        VkPipelineMultisampleStateCreateInfo m_multisample{};
        VkPipelineDepthStencilStateCreateInfo m_depthStencil{};

        std::vector<VkPipelineColorBlendAttachmentState> m_colorBlendAttachments;
        VkPipelineColorBlendStateCreateInfo m_colorBlendState{};

        std::vector<VkDynamicState> m_pipelineDynamicStates;
        VkPipelineDynamicStateCreateInfo m_dynamicState{};

        //Because Blitzen is using dynamic rendering, the pipelines will be initialized with this instead of a VkRenderPass
        VkPipelineRenderingCreateInfo m_rendering{};

        //Since this class might create many pipelines, it will hold a reference to the device to call vkCreatePipelines
        VkDevice* m_pDevice;
    };
}