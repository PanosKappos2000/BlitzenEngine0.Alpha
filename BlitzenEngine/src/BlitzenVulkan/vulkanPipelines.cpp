#include "vulkanPipelines.h"
#include "vulkanSDKobjects.h"

namespace BlitzenRendering
{
    VulkanGraphicsPipelineBuilder::VulkanGraphicsPipelineBuilder()
        :m_pPipelineLayout{nullptr}, m_pGraphicsPipeline{nullptr}, m_pDevice{nullptr}
    {

    }

    void VulkanGraphicsPipelineBuilder::Build()
    {
        VkGraphicsPipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.pNext = &m_rendering;

        info.stageCount = 2;
        info.pStages = m_shaderStages.data();

        info.pVertexInputState = &m_vertexInput;
        info.pInputAssemblyState = &m_inputAssembly;
        info.pTessellationState = &m_tessellation;
        info.pViewportState = &m_viewport;
        info.pRasterizationState = &m_rasterization;
        info.pMultisampleState = &m_multisample;
        info.pDepthStencilState = &m_depthStencil;
        info.pColorBlendState = &m_colorBlendState;
        info.pDynamicState = &m_dynamicState;

        info.layout = *m_pPipelineLayout;

        vkCreateGraphicsPipelines(*m_pDevice, nullptr, 1, &info, nullptr, m_pGraphicsPipeline);
    }

    void VulkanGraphicsPipelineBuilder::Clear()
    {
        m_pGraphicsPipeline = nullptr;
        m_pPipelineLayout = nullptr;

        m_shaderStages[0] = {};
        m_shaderStages[1] = {};

        m_vertexInput = {};
        m_inputAssembly = {};
        m_tessellation = {};
        m_viewport = {};
        m_rasterization = {};
        m_multisample = {};
        m_depthStencil = {};

        m_colorBlendAttachments.clear();
        m_colorBlendState = {};

        m_dynamicState = {};

        m_pipelineDynamicStates.clear();

        m_rendering = {};
    }

    void VulkanGraphicsPipelineBuilder::BuildBasicOpaqueSurfacePipeline(VkPipeline* graphicsPipeline, 
    VkPipelineLayout* pipelineLayout, VkFormat* pColorAttachmentFormats, uint32_t colorAttachmentCount, 
    VkFormat pDepthAttachmentFormat, VkFormat pStencilAttachmentFormat)
    {
        Clear();

        m_pGraphicsPipeline = graphicsPipeline;
        m_pPipelineLayout = pipelineLayout;

        VkPushConstantRange pushConstantRange{};
        VulkanSDKobjects::PushConstantRangeInit(pushConstantRange, sizeof(GPUPushConstant), 
        VK_SHADER_STAGE_VERTEX_BIT);
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        VulkanSDKobjects::PipelineLayoutCreateInfoInit(pipelineLayoutInfo, nullptr, 0, &pushConstantRange, 1);
        vkCreatePipelineLayout(*m_pDevice, &pipelineLayoutInfo, nullptr, pipelineLayout);

        //Get the vertex shader code and wrap in in a vkShaderModule object
        std::array<VkShaderModule, 2> shaderModules;
        std::vector<char> vertShaderCode;
        ReadShaderFile(VULKAN_OPAQUE_GEOMETRY_VERTEX_SHADER_FILENAME, vertShaderCode);
        VkShaderModuleCreateInfo vertShaderModuleInfo{};
        VulkanSDKobjects::ShaderModuleCreateInfoInit(vertShaderModuleInfo, vertShaderCode);
        vkCreateShaderModule(*m_pDevice, &vertShaderModuleInfo, nullptr, &shaderModules[0]);

        //Do the same for the fragment shader
        std::vector<char> fragShaderCode;
        ReadShaderFile(VULKAN_OPAQUE_GEOMETRY_FRAGMENT_SHADER_FILENAME, fragShaderCode);
        VkShaderModuleCreateInfo fragShaderModuleInfo{};
        VulkanSDKobjects::ShaderModuleCreateInfoInit(fragShaderModuleInfo, fragShaderCode);
        vkCreateShaderModule(*m_pDevice, &fragShaderModuleInfo, nullptr, &shaderModules[1]);

        //Create the vertex shader stage
        VulkanSDKobjects::PipelineShaderStageInit(m_shaderStages[0], shaderModules[0], VK_SHADER_STAGE_VERTEX_BIT);
        //Create the fragment shader stage
        VulkanSDKobjects::PipelineShaderStageInit(m_shaderStages[1], shaderModules[1], VK_SHADER_STAGE_FRAGMENT_BIT);

        //Blitzen engine will not be using the vertex input state, it will replace it with storage buffers and gpu pointers
        m_vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        //Set the input assembly state for triangle topology with no primitive restart
        SetBasicTriangleTopology();

        //This pipeline has no need for the tesselation state
        m_tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

        //The viewport will be dynamically set each time a frame is drawn
        SetupDynamicViewport();

        //Automatically sets the polygon mode to fill and the line with to 1
        VulkanSDKobjects::PipelineRasterizationCreateInfoSetPolygonMode(m_rasterization);
        //No back face culling for now
        VulkanSDKobjects::PipelineRasterizationCreateInfoSetCullMode(m_rasterization, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);

        //No multisampling for now
        VulkanSDKobjects::PipelineMultisampleStateCreateInfoInit(m_multisample, VK_SAMPLE_COUNT_1_BIT);

        //Depth test enabled and will be using reverse far and near
        VulkanSDKobjects::PipelineDepthStencilStateCreateInfoSetDepthTest(m_depthStencil, VK_TRUE, VK_TRUE, 
        VK_COMPARE_OP_GREATER_OR_EQUAL);
        //No depth bounds test
        VulkanSDKobjects::PipelineDepthStencilStateCreateInfoSetDepthBoundsTest(m_depthStencil);
        //No depth stencil test
        VulkanSDKobjects::PipelineDepthStencilStateCreateInfoSetStencilTest(m_depthStencil);

        //No color blending used but the attachment still needs to be initialized
        m_colorBlendAttachments.resize(1);
        VulkanSDKobjects::PipelineColorBlendAttachmentStateInit(m_colorBlendAttachments[0]);
        VulkanSDKobjects::PipelineColorBlendStateCreateInfoInit(m_colorBlendState, &m_colorBlendAttachments[0], 1);

        VulkanSDKobjects::PipelineDynamicStateCreateInfoInit(m_dynamicState, m_pipelineDynamicStates.data(),
        static_cast<uint32_t>(m_pipelineDynamicStates.size()));

        VulkanSDKobjects::PipelineRenderingCreateInfoInit(m_rendering, pColorAttachmentFormats, pDepthAttachmentFormat, 
        pStencilAttachmentFormat, colorAttachmentCount);

        Build();

        //Since the graphics pipeline has now been initialized the shader modules can be safely destroyed
        vkDestroyShaderModule(*m_pDevice, shaderModules[0], nullptr);
        vkDestroyShaderModule(*m_pDevice, shaderModules[1], nullptr);
    }

    void VulkanGraphicsPipelineBuilder::StartSettingUp(VkPipeline* pPipeline, VkPipelineLayout* pLayout, 
    VkDevice* pDevice)
    {
        Clear();

        m_pGraphicsPipeline = pPipeline;
        m_pPipelineLayout = pLayout;
        m_pDevice = pDevice;
    }

    void VulkanGraphicsPipelineBuilder::LoadShader(const char* filename, std::vector<char>& codeArray, 
    VkShaderModule& module, uint32_t stageIndex, VkShaderStageFlagBits shaderStage)
    {
        ReadShaderFile(filename, codeArray);

        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = static_cast<uint32_t>(codeArray.size());
        moduleInfo.pCode = reinterpret_cast<uint32_t*>(codeArray.data());
        vkCreateShaderModule(*m_pDevice, &moduleInfo, nullptr, &module);

        m_shaderStages[stageIndex].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_shaderStages[stageIndex].module = module;
        m_shaderStages[stageIndex].stage = shaderStage;
        m_shaderStages[stageIndex].pName = "main";
    }

    void VulkanGraphicsPipelineBuilder::ReadShaderFile(const char* filename, std::vector<char>& shaderCode)
    {
        //Initialize a file reader for the .spv in binary format and with the cursor at the end of the file
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            std::cerr << "BLITZEN_VULKAN::SHADERS::FAILURE_TO_LOAD_COMPILED_SHADERS\n"
            << "The path to all compiled vulkan shader files is BlitzenEngine/VulkanShaders, \n"
            << "the application must be executed from somewhere that makes this filepath valid";
        	__debugbreak();
        }


        size_t filesize = static_cast<size_t>(file.tellg());

        shaderCode.resize(filesize);

        //put the file cursor at the beginning
        file.seekg(0);

        // load the entire file into the array
        file.read(shaderCode.data(), filesize);

        file.close();
    }

    void VulkanGraphicsPipelineBuilder::SetupDefaults()
    {
        m_vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        m_tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    }

    void VulkanGraphicsPipelineBuilder::SetPolygonMode(VkPolygonMode polygonMode, float lineWidth /* =1.0f */)
    {
        m_rasterization.polygonMode = polygonMode;
	    m_rasterization.lineWidth = lineWidth;
    }

    void VulkanGraphicsPipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
    {
        m_rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	    m_rasterization.rasterizerDiscardEnable = VK_FALSE;
	    m_rasterization.cullMode = cullMode;
	    m_rasterization.frontFace = frontFace;
    }

    void VulkanGraphicsPipelineBuilder::SetBasicTriangleTopology()
    {
        m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        m_inputAssembly.primitiveRestartEnable = VK_FALSE;
        m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    void VulkanGraphicsPipelineBuilder::SetupDynamicViewport()
    {
        m_viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        m_viewport.scissorCount = 1;
        m_viewport.viewportCount = 1;

        m_pipelineDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        m_pipelineDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

        VulkanSDKobjects::PipelineDynamicStateCreateInfoInit(m_dynamicState, m_pipelineDynamicStates.data(),
        static_cast<uint32_t>(m_pipelineDynamicStates.size()));
    }

    void VulkanGraphicsPipelineBuilder::DisableMultisampling()
    {
        m_multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	    m_multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	    m_multisample.sampleShadingEnable = VK_FALSE;
	    m_multisample.minSampleShading = 1.0f;
	    m_multisample.pSampleMask = nullptr;
	    m_multisample.alphaToCoverageEnable = VK_FALSE;
	    m_multisample.alphaToOneEnable = VK_FALSE;
    }

    void VulkanGraphicsPipelineBuilder::EnableDepthTest(VkCompareOp compareOp /* =VK_COMPARE_OP_GREATER_OR_EQUAL */)
    {
        m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	    m_depthStencil.depthTestEnable = VK_TRUE;
	    m_depthStencil.depthWriteEnable = VK_TRUE;
	    m_depthStencil.depthCompareOp = compareOp;
    }

    void VulkanGraphicsPipelineBuilder::SetDepthBoundsTest(VkBool32 bDepthBounds /* =VK_FALSE */, 
    float minDepthBounds /* =0.f */, float maxDepthBounds /* =1.f */)
    {
        m_depthStencil.depthBoundsTestEnable = bDepthBounds;
	    m_depthStencil.minDepthBounds = minDepthBounds;
	    m_depthStencil.maxDepthBounds = maxDepthBounds;
    }

    void VulkanGraphicsPipelineBuilder::DisableColorBlending()
    {
        m_colorBlendAttachments.resize(1);
        m_colorBlendAttachments[0].blendEnable = VK_FALSE;
        m_colorBlendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        m_colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	    m_colorBlendState.attachmentCount = 1;
	    m_colorBlendState.pAttachments = m_colorBlendAttachments.data();
	    m_colorBlendState.logicOpEnable = VK_FALSE;
    }

    void VulkanGraphicsPipelineBuilder::SetupRenderingInfo(VkFormat* pColorAttachmentFormats, 
    uint32_t colorAttachmentCount, VkFormat depthAttachmentFormat, VkFormat stencilAttachmentFormat)
    {
        m_rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        m_rendering.colorAttachmentCount = colorAttachmentCount;
        m_rendering.pColorAttachmentFormats = pColorAttachmentFormats;
        m_rendering.depthAttachmentFormat = depthAttachmentFormat;
        m_rendering.stencilAttachmentFormat = stencilAttachmentFormat;
    }

    void VulkanGraphicsPipelineBuilder::SetupPipelineLayout(VkPushConstantRange* pPushConstants, 
    uint32_t pushConstantCount, VkDescriptorSetLayout* pDescriptorLayouts, uint32_t descriptorLayoutCount)
    {
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = descriptorLayoutCount;
        layoutInfo.pSetLayouts = pDescriptorLayouts;
        layoutInfo.pPushConstantRanges = pPushConstants;
        layoutInfo.pushConstantRangeCount = pushConstantCount;

        vkCreatePipelineLayout(*m_pDevice, &layoutInfo, nullptr, m_pPipelineLayout);
    }
}