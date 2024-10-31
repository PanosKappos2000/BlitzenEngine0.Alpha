#include "VulkanRenderer.h"

//Includes the Vulkan Memory Allocator with function definitions
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

//Includes functions that initalize some key Vulkan SDK structs
#include "vulkanSDKobjects.h"

namespace BlitzenRendering
{
    VulkanRenderer::VulkanRenderer()
    {

    }

    void VulkanRenderer::Init(WindowData* pWindowData)
    {
        vkb::Instance vkbInstance = BootstrapCreateInstance();

        //Passing the window data, since the GLFW window will be needed to create the window surface
        m_pWindowData = pWindowData;

        //Window surface created before GPU selection, so that the surface can be passed to the GPU handle
        glfwCreateWindowSurface(m_bootstrapObjects.vulkanInstance, 
            m_pWindowData->pWindow, nullptr, &(m_bootstrapObjects.windowSurface));

        //GPU selection and device creation
        BootstrapSelectGPUAndCreateDevice(vkbInstance);

        //Initialize the allocator for buffers and images
        InitAllocator();

        //Intialize the swapchain
        BootstrapCreateSwapchain();

        //Allocate the command buffer that will be used for quick commands outside the main loop
        m_instantSubmit.Init(m_device, m_queues.graphicsQueueFamilyIndex, &(m_queues.graphicsQueue));

        //Create the command buffers, semaphores and fences that will be used in the draw frame function
        InitFrameTools();

        //Allocate the color attachment
        AllocateImage(m_colorAttachmentImage, {(uint32_t)m_pWindowData->windowWidth, (uint32_t)m_pWindowData->windowHeight, 1}, 
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

        //Allocate the depth stencil attachment
        AllocateImage(m_depthAttachmentImage, m_colorAttachmentImage.extent, VK_FORMAT_D32_SFLOAT, 
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    void VulkanRenderer::InitPlaceholderData()
    {
        //Initialize the graphics pipeline builder and build a basic pipeline to draw the triangle
        m_graphicsPipelineBuilder.Init(&m_device);

        InitPlaceholderMaterial();
        m_placeholderMaterial.pPipeline = &(m_placeholderMaterialData.opaquePipeline);

        for(size_t i = 0; i < m_assets.size(); ++i)
        {
            //Create a new mesh node
            m_nodeTable[m_assets[i].meshName] = Node();
            Node& newNode = m_nodeTable[m_assets[i].meshName];

            //Assign the current mesh asset to the node
            newNode.m_asset = &m_assets[i];
            newNode.type = NodeType::NT_MeshNode;

            newNode.localTransform = glm::mat4{ 1.f };
		    newNode.worldTransform = glm::mat4{ 1.f };

            for(GeoSurface& surface : newNode.m_asset->geoSurfaces)
            {
                surface.pMaterial = &m_placeholderMaterial;
            }

            
        }
    }

    vkb::Instance VulkanRenderer::BootstrapCreateInstance()
    {
        vkb::InstanceBuilder vkbInstanceBuilder;

        vkbInstanceBuilder.set_app_name("Blitzen Vulkan Renderer");
	    vkbInstanceBuilder.request_validation_layers(m_bootstrapObjects.bEnableValidationLayers); 
	    vkbInstanceBuilder.use_default_debug_messenger();
	    vkbInstanceBuilder.require_api_version(1, 3, 0);

        //VkbInstance built to initialize instance and debug messenger
        auto vkbInstanceBuilderResult = vkbInstanceBuilder.build();
        vkb::Instance vkbInstance = vkbInstanceBuilderResult.value();

        //Instance reference from vulkan data initialized
        m_bootstrapObjects.vulkanInstance = vkbInstance.instance;

        //Debug Messenger reference from vulkan data initialized
        m_bootstrapObjects.debugMessenger = vkbInstance.debug_messenger;

        //The vkbInstance will be used by VkBootstrap again to select the GPU
        return vkbInstance;
    }

    void VulkanRenderer::BootstrapSelectGPUAndCreateDevice(const vkb::Instance& vkbInstance)
    {
        //Setting desired vulkan 1.3 features
        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        //Using dynamic rendering since the engine will not benefit from the VkRenderPass
        vulkan13Features.dynamicRendering = true;
        vulkan13Features.synchronization2 = true;

        //Setting desired vulkan 1.2 features
        VkPhysicalDeviceVulkan12Features vulkan12Features{};
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        //Will allow us to create GPU pointers to access storage buffers
        vulkan12Features.bufferDeviceAddress = true;
        vulkan12Features.descriptorIndexing = true;

        //vkbDeviceSelector built with reference to vkbInstance built earlier
        vkb::PhysicalDeviceSelector vkbDeviceSelector{ vkbInstance };
        vkbDeviceSelector.set_minimum_version(1, 3);
        vkbDeviceSelector.set_required_features_13(vulkan13Features);
        vkbDeviceSelector.set_required_features_12(vulkan12Features);
        vkbDeviceSelector.set_surface(m_bootstrapObjects.windowSurface);

        //Selecting the GPU and giving its value to the vkb::PhysicalDevice handle
        vkb::PhysicalDevice vkbPhysicalDevice = vkbDeviceSelector.select().value();

        //Saving the actual vulkan gpu handle 
        m_bootstrapObjects.chosenGPU = vkbPhysicalDevice.physical_device;

        //Setting up the vkDevice based on the chosen gpu
        vkb::DeviceBuilder vkbDeviceBuilder{ vkbPhysicalDevice };
        vkb::Device vkbDevice = vkbDeviceBuilder.build().value();
        //Savign the actual vulkan device
        m_device = vkbDevice.device;

        //Setting up the graphics queue and its queue family index
        m_queues.graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        m_queues.graphicsQueueFamilyIndex = vkbDevice.get_queue_index(
            vkb::QueueType::graphics).value();

        //Setting up the present queue and its queue family index
        m_queues.presentQueue = vkbDevice.get_queue(vkb::QueueType::present).value();
        m_queues.presentQueueFamilyIndex = vkbDevice.get_queue_index(
            vkb::QueueType::present).value();
    }

    void VulkanRenderer::InitAllocator()
    {
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.device = m_device;
        allocatorInfo.instance = m_bootstrapObjects.vulkanInstance;
        allocatorInfo.physicalDevice = m_bootstrapObjects.chosenGPU;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        vmaCreateAllocator(&allocatorInfo, &m_allocator);
    }

    void VulkanRenderer::BootstrapCreateSwapchain()
    {
        vkb::SwapchainBuilder vkSwapBuilder{ m_bootstrapObjects.chosenGPU, m_device, m_bootstrapObjects.windowSurface };

        //Setting the desired image format
        m_bootstrapObjects.swapchainData.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;

        vkb::Result<vkb::Swapchain> vkbSwapBuilderResult = vkSwapBuilder.set_desired_format(VkSurfaceFormatKHR{ 
            m_bootstrapObjects.swapchainData.imageFormat, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }) //Setting the desrired surface format
        	.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)//Setting the present mode to be limited to the speed of the monitor
        	.set_desired_extent(m_pWindowData->windowWidth, m_pWindowData->windowHeight)
        	.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        	.build();

        //Building the vkb swapchain so that the swapchain data can be retrieved
        vkb::Swapchain vkbSwapchain = vkbSwapBuilderResult.value();

        //Setting the actual vulkan swapchain struct
        m_bootstrapObjects.swapchainData.swapchain = vkbSwapchain.swapchain;

        //Saving the swapchain's window extent
        m_bootstrapObjects.swapchainData.swapchainExtent = vkbSwapchain.extent;

        //Saving the swapchain images, they will be used each frame to present the render result to the swapchain
        m_bootstrapObjects.swapchainData.swapchainImages = vkbSwapchain.get_images().value();

        //Saving the swapchain image views, they will be used to access each swapchain image
        m_bootstrapObjects.swapchainData.swapchainImageViews = vkbSwapchain.get_image_views().value();
    }




    void VulkanRenderer::InitFrameTools()
    {
        VkCommandPoolCreateInfo commandPoolInfo{};
        //Each command buffer will be created so that it can be individually reset
        VulkanSDKobjects::CommandPoolCreateInfoInit(commandPoolInfo, m_queues.graphicsQueueFamilyIndex, 
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        VulkanSDKobjects::SemaphoreCreateInfoInit(semaphoreInfo);

        //The in flight fences will be creted in the signaled state, so that the 1st does not deadlock
        VkFenceCreateInfo fenceInfo{};
        VulkanSDKobjects::FenceCreateInfoInit(fenceInfo, VK_FENCE_CREATE_SIGNALED_BIT);

        for(size_t i = 0; i < m_frameToolList.size(); ++i)
        {
            vkCreateFence(m_device, &fenceInfo, nullptr, &(m_frameToolList[i].inFlightFence));

            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &(m_frameToolList[i].renderFinishedSemaphore));

            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &(m_frameToolList[i].imageAvailableSemaphore));

            vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &(m_frameToolList[i].renderCommandPool));

            /*-----------------------------------------------------------------------
            Since each command buffer will be allocated by a different command pool, 
            every command buffer will have a different create info struct
            -------------------------------------------------------------------------*/
            VkCommandBufferAllocateInfo commandBufferInfo{};
            VulkanSDKobjects::CommandBufferAllocInfoInit(commandBufferInfo, (m_frameToolList[i].renderCommandPool), 
            VK_COMMAND_BUFFER_LEVEL_PRIMARY);
            vkAllocateCommandBuffers(m_device, &commandBufferInfo, &(m_frameToolList[i].renderCommandBuffer));

            m_frameToolList[i].descriptorAllocator.Init(m_device);

            AllocateBuffer(m_frameToolList[i].sceneDataBuffer, sizeof(GPUSceneData), 
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        }
    }




    void OneTimeCommands::Init(const VkDevice& m_device, uint32_t queueFamilyIndex, VkQueue* queue)
    {
        submitQueue = queue;
        submitQueueFamilyIndex = queueFamilyIndex;

        VkCommandPoolCreateInfo commandPoolInfo{};
        VulkanSDKobjects::CommandPoolCreateInfoInit(commandPoolInfo, queueFamilyIndex, 
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &commandPool);

        VkCommandBufferAllocateInfo commandBufferInfo{};
        VulkanSDKobjects::CommandBufferAllocInfoInit(commandBufferInfo, commandPool, 
        VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        vkAllocateCommandBuffers(m_device, &commandBufferInfo, &commandBuffer);
    }

    void OneTimeCommands::StartRecording()
    {
        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo begin{};
        VulkanSDKobjects::CommandBufferBeginInfoInit(begin, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vkBeginCommandBuffer(commandBuffer, &begin);
    }

    void OneTimeCommands::EndRecordingAndSubmit()
    {
        vkEndCommandBuffer(commandBuffer);

        VkCommandBufferSubmitInfo commandBufferSubmit{};
        VulkanSDKobjects::CommandBufferSubmitInfoInit(commandBufferSubmit, commandBuffer);

        VkSubmitInfo2 queueSubmit{};
        queueSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        queueSubmit.commandBufferInfoCount = 1;
        queueSubmit.pCommandBufferInfos = &commandBufferSubmit;

        vkQueueSubmit2(*submitQueue, 1, &queueSubmit, nullptr);
        vkQueueWaitIdle(*submitQueue);
    }




    void VulkanRenderer::AllocateImage(VulkanAllocatedImage& imageToAllocate, VkExtent3D imageExtent, 
        VkFormat imageFormat, VkImageUsageFlags imageUsage, bool bMipmapped /* =false */)
    {
        //Save the image extent and image format to the allocated image structure
        imageToAllocate.extent = imageExtent;
        imageToAllocate.format = imageFormat;

        //Create the Vulkan SDK vkImageCreateInfo object with the parameters given
        VkImageCreateInfo imageToAllocateInfo{};
        VulkanSDKobjects::ImageCreateInfoInit(imageToAllocateInfo, imageExtent, imageFormat, imageUsage);

        //Create the allocation info for vma 
        VmaAllocationCreateInfo imageAllocationInfo{};
        imageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        imageAllocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        //Allocate the image
        vmaCreateImage(m_allocator, &imageToAllocateInfo, &imageAllocationInfo, &(imageToAllocate.image), 
        &(imageToAllocate.allocation), nullptr);

        //The aspect flags that will be used for the image view depend on if the image is a depth attahcment or not
        VkImageAspectFlags imageAspect{};
        (imageFormat == VK_FORMAT_D32_SFLOAT) ? imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT : imageAspect = 
        VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageViewCreateInfo imageViewInfo{};
        VulkanSDKobjects::ImageViewCreateInfoInit(imageViewInfo, imageToAllocate.image, imageAspect, imageFormat);

        //Create the image view
        vkCreateImageView(m_device, &imageViewInfo, nullptr, &(imageToAllocate.imageView));
    }




    void VulkanRenderer::LoadMeshBuffers(VulkanGPUMeshBuffers& meshBuffers, std::vector<VulkanVertex>& vertices, 
        std::vector<uint32_t>& indices)
    {
        VkDeviceSize vertexBufferSize = sizeof(VulkanVertex) * vertices.size();
        /*----------------------------------------------------------------------------------------
        Create the vertex buffer as an SSBO (that will accept a transfer from a staging buffer), 
        its memory will only be accessed by the GPU but shaders will have access
        -----------------------------------------------------------------------------------------*/
        AllocateBuffer(meshBuffers.vertexBuffer, vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
        //The index buffer will have the index buffer bit and will also accept a memory transfer
        AllocateBuffer(meshBuffers.indexBuffer, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        /*---------------------------------------------------------------------------------------------------
        Since the vertex buffer is only available in the gpu(shaders), the meshBuffers to save its address 
        to give it to the push constants so that the geometry can actually be drawn
        ----------------------------------------------------------------------------------------------------*/
        VkBufferDeviceAddressInfo vertexBufferAddressInfo{};
        vertexBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        vertexBufferAddressInfo.buffer = meshBuffers.vertexBuffer.buffer;
        meshBuffers.vertexBufferAddress = vkGetBufferDeviceAddress(m_device, &vertexBufferAddressInfo);

        //The buffers have been created, now the data needs to be passed to their gpu memory using a staging buffer
        VulkanAllocatedBuffer stagingBuffer;
        AllocateBuffer(stagingBuffer, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VMA_MEMORY_USAGE_CPU_ONLY);

        //Map a void pointer to the staging buffer's memory, so that the vertex data can be loaded
        void* data = stagingBuffer.allocation->GetMappedData();

        //Put the vertices at the start of the memory address
        memcpy(data, vertices.data(), vertexBufferSize);
        //Place the indices after the vertices
        memcpy(reinterpret_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);

        //Start recording copy commands
        m_instantSubmit.StartRecording();

        //Copy the vertices into the vertex buffer
        VkBufferCopy vertexBufferCopyRegion{0};
        vertexBufferCopyRegion.dstOffset = 0;
        vertexBufferCopyRegion.srcOffset = 0;
        vertexBufferCopyRegion.size = vertexBufferSize;
        vkCmdCopyBuffer(m_instantSubmit.commandBuffer, stagingBuffer.buffer, meshBuffers.vertexBuffer.buffer, 1, 
        &vertexBufferCopyRegion);

        //Copy the indices into the index buffer
        VkBufferCopy indexBufferCopyRegion{0};
        indexBufferCopyRegion.dstOffset = 0;
        indexBufferCopyRegion.srcOffset = vertexBufferSize;
        indexBufferCopyRegion.size = indexBufferSize;
        vkCmdCopyBuffer(m_instantSubmit.commandBuffer, stagingBuffer.buffer, meshBuffers.indexBuffer.buffer, 1, 
        &indexBufferCopyRegion);

        //Submit the commands
        m_instantSubmit.EndRecordingAndSubmit();

        //Free the memory of the staging buffer
        vmaDestroyBuffer(m_allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }

    void VulkanRenderer::AllocateBuffer(VulkanAllocatedBuffer& bufferToAllocate, VkDeviceSize bufferSize, 
    VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage)
    {
        //Create the buffer's info
        VkBufferCreateInfo bufferInfo{};
        VulkanSDKobjects::BufferCreateInfoInit(bufferInfo, bufferSize, bufferUsage);

        //Create the allocation info
        VmaAllocationCreateInfo bufferAllocationInfo{};
        bufferAllocationInfo.usage = memoryUsage;
        //All buffer allocations will create a pointer to the allocation, available in the allocation info
        bufferAllocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        //Have vma allocate the buffer
        vmaCreateBuffer(m_allocator, &bufferInfo, &bufferAllocationInfo, &(bufferToAllocate.buffer), 
        &(bufferToAllocate.allocation), &(bufferToAllocate.allocationInfo));
    }




    void VulkanRenderer::InitPlaceholderMaterial()
    {
        m_graphicsPipelineBuilder.StartSettingUp(&(m_placeholderMaterialData.opaquePipeline.graphicsPipeline), 
        &(m_placeholderMaterialData.opaquePipeline.pipelineLayout), &m_device);

        //Set up the vertex shader stage
        std::vector<char> vertShaderCode;
        VkShaderModule vertShaderModule;
        m_graphicsPipelineBuilder.LoadShader(VULKAN_OPAQUE_GEOMETRY_VERTEX_SHADER_FILENAME, 
        vertShaderCode, vertShaderModule, 0, VK_SHADER_STAGE_VERTEX_BIT);

        //Set up the fragment shader stage
        std::vector<char> fragShaderCode;
        VkShaderModule fragShaderModule;
        m_graphicsPipelineBuilder.LoadShader(VULKAN_OPAQUE_GEOMETRY_FRAGMENT_SHADER_FILENAME, 
        fragShaderCode, fragShaderModule, 1, VK_SHADER_STAGE_FRAGMENT_BIT);

        //Give default values for the stages that the engine does not need(vertex input and tessellation for now)
        m_graphicsPipelineBuilder.SetupDefaults();

        //Setup the polygon mode in the rasterization state and give the edge lines a standard width of 1
        m_graphicsPipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL, 1.0f);

        //Set the culling mode in the rasterization state
        m_graphicsPipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);

        //Set input assembly for triangle topology
        m_graphicsPipelineBuilder.SetBasicTriangleTopology();

        //Set the viewport so that it is part of the dynamic state
        m_graphicsPipelineBuilder.SetupDynamicViewport();

        //Not using multismapling for now
        m_graphicsPipelineBuilder.DisableMultisampling();

        //Depth testing with default settings
        m_graphicsPipelineBuilder.EnableDepthTest();

        //Not depth bounds test
        m_graphicsPipelineBuilder.SetDepthBoundsTest();

        //No color blending for opaque objects
        m_graphicsPipelineBuilder.DisableColorBlending();

        //Pass the formats of the rendering attachments
        m_graphicsPipelineBuilder.SetupRenderingInfo(&(m_colorAttachmentImage.format), 1, 
        m_depthAttachmentImage.format, VK_FORMAT_UNDEFINED);

        //Setup push constants for the pipeline layout
        VkPushConstantRange pushConstants{};
        pushConstants.offset = 0;
        pushConstants.size = sizeof(GPUPushConstant);
        pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        //Set binding for material constants uniform buffer
        VkDescriptorSetLayoutBinding uniformBufferBinding{};
        uniformBufferBinding.binding = 0;
        uniformBufferBinding.descriptorCount = 1;
        uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformBufferBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        //Set descriptor set binding for material texture image sampler(normal color version)
        VkDescriptorSetLayoutBinding normalCombinedImageSamplerBinding{};
        normalCombinedImageSamplerBinding.binding = 1;
        normalCombinedImageSamplerBinding.descriptorCount = 1;
        normalCombinedImageSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        normalCombinedImageSamplerBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;   

        //Set descriptor set binding for material texture image sampler(metallic property version)
        VkDescriptorSetLayoutBinding metallicCombinedImageSamplerBinding{};
        metallicCombinedImageSamplerBinding.binding = 2;
        metallicCombinedImageSamplerBinding.descriptorCount = 1;
        metallicCombinedImageSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        metallicCombinedImageSamplerBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 3> descriptorSetBindings = 
        {
            uniformBufferBinding, normalCombinedImageSamplerBinding, metallicCombinedImageSamplerBinding
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
        descriptorSetLayoutInfo.bindingCount = 3; 
        descriptorSetLayoutInfo.pBindings = descriptorSetBindings.data();
        descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutInfo, nullptr, &(m_placeholderMaterialData.materialLayout));

        //Creating the descriptor layout for the global scene data, I will have to move this to a different function at some point
        VkDescriptorSetLayoutBinding sceneDataDescriptorSetLayoutBinding{};
        VulkanSDKobjects::DescriptorSetLayoutBindingInit(sceneDataDescriptorSetLayoutBinding, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        VkDescriptorSetLayoutCreateInfo sceneDataDescriptorSetLayoutInfo{};
        VulkanSDKobjects::DescriptorSetLayoutCreateInfoInit(sceneDataDescriptorSetLayoutInfo, 1, 
        &sceneDataDescriptorSetLayoutBinding, 0);

        vkCreateDescriptorSetLayout(m_device, &sceneDataDescriptorSetLayoutInfo, nullptr, &m_globalSceneDataDescriptorSetLayout);

        std::array<VkDescriptorSetLayout, 2> descriptorSetLayout =
        {
            m_globalSceneDataDescriptorSetLayout, m_placeholderMaterialData.materialLayout
        };

        m_graphicsPipelineBuilder.SetupPipelineLayout(&pushConstants, 1, descriptorSetLayout.data(), 2);

        m_graphicsPipelineBuilder.Build();

        vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
        vkDestroyShaderModule(m_device, fragShaderModule, nullptr);  
    }

    void VulkanRenderer::WriteMaterial(MaterialInstance& instance, VkDevice device, MaterialPass pass, 
    MaterialResources& resources)
    {
        instance.pass = pass;
        if(pass == MaterialPass::MP_opaqueMaterial)
        {
            instance.pPipeline = &(m_placeholderMaterialData.opaquePipeline);
        }
        else if(pass == MaterialPass::MP_transparentMaterial)
        {
            instance.pPipeline = &(m_placeholderMaterialData.transparentPipeline);
        }

        m_descriptorWriter.Clear();
        m_descriptorWriter.WriteBuffer(0, resources.dataBuffer, 
        sizeof(MaterialConstants), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        m_descriptorWriter.WriteImage(1, resources.colorImage.imageView, resources.colorSampler, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        m_descriptorWriter.WriteImage(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        m_descriptorWriter.UpdateSet(m_device, instance.descriptorSet);
    }




    void DescriptorAllocator::Init(const VkDevice& device)
    {
        CreateDescriptorPool(device);
    }

    void DescriptorAllocator::CreateDescriptorPool(const VkDevice& device)
    {
        //The amount of descriptors of each type is standard and each pool created can hold this many
        std::array<VkDescriptorPoolSize, 4> poolSizes;
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 5;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 4;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[2].descriptorCount = 4;
        poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[3].descriptorCount = 3;

        //Create a new descriptor pool at the end of the vertex buffer
        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        VulkanSDKobjects::DescriptorPoolCreateInfoInit(descriptorPoolInfo, 1000, 4, poolSizes.data());
        readyPools.resize(readyPools.size() + 1);
        vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &readyPools[readyPools.size() - 1]);
    }

    void DescriptorAllocator::AllocateDescriptorSet(const VkDevice& device, 
    VkDescriptorSet& descriptorSetToAllocate, VkDescriptorSetLayout& layout)
    {
        //Get an index for a ready pool to use
        size_t readyPoolIndex = GetDescriptorPoolIndex(device);

        //Pass the descriptor pool to the allocation info
        VkDescriptorSetAllocateInfo setInfo{};
        VulkanSDKobjects::DescriptorSetAllocateInfoInit(setInfo, readyPools[readyPoolIndex], 
        &layout);

        //Check if the allocation was succesful
        VkResult initialAllocationResult = vkAllocateDescriptorSets(device, &setInfo, &descriptorSetToAllocate);
        if(initialAllocationResult == VK_ERROR_OUT_OF_POOL_MEMORY || initialAllocationResult == VK_ERROR_FRAGMENTED_POOL)
        {
            //If the allocation failes, get a new index and add the problematic pool to the full pools array
            fullPools.push_back(readyPools[readyPoolIndex]);
            readyPools.pop_back();
            readyPoolIndex = GetDescriptorPoolIndex(device);
            setInfo.descriptorPool = readyPools[readyPoolIndex];

            //If the allocation fails again, warn that the allocator is faulty
            VkResult finalCheck = vkAllocateDescriptorSets(device, &setInfo, &descriptorSetToAllocate);
            if(finalCheck == VK_ERROR_OUT_OF_POOL_MEMORY || finalCheck == VK_ERROR_FRAGMENTED_POOL)
            {
                std::cout << "Descriptor Allocator has been compromised\n";
            }
        }
    }

    size_t DescriptorAllocator::GetDescriptorPoolIndex(const VkDevice& device)
    {
        if(readyPools.size() != 0)
        {
            return readyPools.size() - 1;
        }
        else
        {
            CreateDescriptorPool(device);
            return readyPools.size() - 1;
        }
    }

    void DescriptorAllocator::ResetPools(const VkDevice& device)
    {
        //Reset all ready pools
        for(size_t i = 0; i < readyPools.size(); ++i)
        {
            vkResetDescriptorPool(device, readyPools[i], 0);
        }

        //Reset the full pools and add them to the ready pools
        for(size_t i = 0; i < fullPools.size(); ++i)
        {
            vkResetDescriptorPool(device, fullPools[i], 0);
            readyPools.push_back(fullPools[i]);
        }
        //Clear the full pools array
        fullPools.clear();
    }

    void DescriptorWriter::WriteBuffer(int binding, VkBuffer& buffer, size_t size, size_t offset, VkDescriptorType type)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = size;
        VkDescriptorBufferInfo& info = bufferInfos.emplace_back(bufferInfo);

	    VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	    write.dstBinding = binding;
	    write.dstSet = VK_NULL_HANDLE; //left empty for now, will be written on update descriptors
	    write.descriptorCount = 1;
	    write.descriptorType = type;
	    write.pBufferInfo = &info;

	    writes.push_back(write);
    }

    void DescriptorWriter::WriteImage(int binding, VkImageView& image, VkSampler& sampler, VkImageLayout layout, 
    VkDescriptorType type)
    {
        VkDescriptorImageInfo imageInfo{};
	    imageInfo.sampler = sampler,
	    imageInfo.imageView = image,
	    imageInfo.imageLayout = layout;
        VkDescriptorImageInfo& info = imageInfos.emplace_back(imageInfo);

	    VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	    write.dstBinding = binding;
	    write.dstSet = VK_NULL_HANDLE; //left empty for now, will be written on update descriptors
	    write.descriptorCount = 1;
	    write.descriptorType = type;
	    write.pImageInfo = &info;

	    writes.push_back(write);
    }

    void DescriptorWriter::Clear()
    {
        imageInfos.clear();
        writes.clear();
        bufferInfos.clear();
    }

    void DescriptorWriter::UpdateSet(const VkDevice& device, VkDescriptorSet& set)
    {
        for(size_t i = 0; i < writes.size(); ++i)
        {
            writes[i].dstSet = set;
        }
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }




    void VulkanRenderer::DrawFrame()
    {
        if(m_pWindowData->bRendererShouldWait)
        {

        }

        UpdateScene();

        /*-----------------------------------------------------------------------------
        Wait for the fence to be signalled at the end of the previous frame, 
        the fence was created with the signaled bit so the first frame won't deadlock
        -------------------------------------------------------------------------------*/
        vkWaitForFences(m_device, 1, &(m_frameToolList[currentFrame].inFlightFence), VK_TRUE, 100000000);

        vkResetFences(m_device, 1, &(m_frameToolList[currentFrame].inFlightFence));

        //Acquiring an image from the swapchain to present the render to the screen
        uint32_t swapchainImageIndex;
        vkAcquireNextImageKHR(m_device, m_bootstrapObjects.swapchainData.swapchain, 100000000, 
        m_frameToolList[currentFrame].imageAvailableSemaphore, VK_NULL_HANDLE, &swapchainImageIndex);

        //Make sure that the command buffer is clean and start recording commands
        vkResetCommandBuffer(m_frameToolList[currentFrame].renderCommandBuffer, 0);
        StartRecordingFrameCommands(m_frameToolList[currentFrame].renderCommandBuffer, swapchainImageIndex);

        //The color attachment parts of the commands should not be executed until an image is available
        VkSemaphoreSubmitInfo waitSemaphoreSubmit{};
        VulkanSDKobjects::SemaphoreSubmitInfoInit(waitSemaphoreSubmit, 
        m_frameToolList[currentFrame].imageAvailableSemaphore, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR);

        //All types of commands that come after submitting the command buffer, should wait for this frame to finish
        VkSemaphoreSubmitInfo signalSemaphoreSubmit{};
        VulkanSDKobjects::SemaphoreSubmitInfoInit(signalSemaphoreSubmit, 
        m_frameToolList[currentFrame].renderFinishedSemaphore, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);

        VkCommandBufferSubmitInfo commandBufferSubmit{};
        VulkanSDKobjects::CommandBufferSubmitInfoInit(commandBufferSubmit, m_frameToolList[currentFrame].renderCommandBuffer);

        //Submitting the command buffer along with sync object configurations
        VkSubmitInfo2 submitInfo{};
        VulkanSDKobjects::SubmitInfo2Init(submitInfo, &waitSemaphoreSubmit, &signalSemaphoreSubmit, &commandBufferSubmit);
        vkQueueSubmit2(m_queues.graphicsQueue, 1, &submitInfo, m_frameToolList[currentFrame].inFlightFence);

        VkPresentInfoKHR presentInfo{};
        VulkanSDKobjects::PresentInfoKHRInit(presentInfo, m_bootstrapObjects.swapchainData.swapchain, &swapchainImageIndex, 
        &m_frameToolList[currentFrame].renderFinishedSemaphore);
        vkQueuePresentKHR(m_queues.presentQueue, &presentInfo);

        //Set the currentFrame to the next one, but make sure it does not go over BLITZEN_MAX_FRAMES_IN_FLIGHT
        currentFrame = (currentFrame +1) % BLITZEN_MAX_FRAMES_IN_FLIGHT;
    }

    void VulkanRenderer::UpdateScene()
    {
        m_mainDrawContext.opaqueObjects.clear();
        m_nodeTable["Suzanne"].AddToDrawContext(glm::mat4(1.f), m_mainDrawContext);

        //Setup the view matrix
        m_globalSceneData.viewMatrix = glm::translate(glm::vec3{ 0,0,-5 });
	    
        //Setup the projection matrix
	    m_globalSceneData.projectionMatrix = glm::perspective(glm::radians(70.f), (float)m_pWindowData->windowWidth / 
        (float)m_pWindowData->windowHeight, 10000.f, 0.1f);

	    //Invert the projection matrix so that it matches glm and objects are not drawn upside down
	    m_globalSceneData.projectionMatrix[1][1] *= -1;

	    //Default lighting parameters
	    m_globalSceneData.ambientColor = glm::vec4(.1f);
	    m_globalSceneData.sunlightColor = glm::vec4(1.f);
	    m_globalSceneData.sunlightDirection = glm::vec4(0,1,0.5,1.f);
    }

    void VulkanRenderer::StartRecordingFrameCommands(const VkCommandBuffer& commandBuffer, 
    uint32_t swapchainImageIndex)
    {
        //Put the command buffer in the ready state
        VkCommandBufferBeginInfo commandBufferBegin{};
        VulkanSDKobjects::CommandBufferBeginInfoInit(commandBufferBegin, 
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        vkBeginCommandBuffer(commandBuffer, &commandBufferBegin);

        //Draw the background
        DrawBackground(commandBuffer);

        //Before rendering geometry the draw extent needs to be set to the size of the window
        m_drawExtent.width = m_pWindowData->windowWidth;
        m_drawExtent.height = m_pWindowData->windowHeight;

        //Change the color attachment's layout so that it fits the next function's needs
        ChangeImageLayout(commandBuffer, m_colorAttachmentImage.image, VK_IMAGE_LAYOUT_GENERAL, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        //Change the depth attachment's layout so that it is ready to be used to draw geometry
        ChangeImageLayout(commandBuffer, m_depthAttachmentImage.image, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        DrawGeometry(commandBuffer);

        //Change the image layout so that it can be used for data transfer
        ChangeImageLayout(commandBuffer, m_colorAttachmentImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        //Change the image layout so that it can accept a data transfer
        ChangeImageLayout(commandBuffer, m_bootstrapObjects.swapchainData.swapchainImages[swapchainImageIndex], 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        //Copy the color attachment image to the swapchain image so that it can be presented on the screen
        CopyImageToImage(commandBuffer, m_colorAttachmentImage.image, 
        m_bootstrapObjects.swapchainData.swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_drawExtent, m_bootstrapObjects.swapchainData.swapchainExtent);

        ChangeImageLayout(commandBuffer, m_bootstrapObjects.swapchainData.swapchainImages[swapchainImageIndex], 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        //Once all commands have been recorded the command buffer can be reset
        vkEndCommandBuffer(commandBuffer);
    }

    void VulkanRenderer::DrawBackground(const VkCommandBuffer& commandBuffer)
    {
        //Chnage the image layout so that it can be used for clearing the color of the screen
        ChangeImageLayout(commandBuffer, m_colorAttachmentImage.image, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_GENERAL);

        VkClearColorValue clearColorValue{};
        clearColorValue = {0.0f, 0.0f, 0.0f, 1.0f};

        VkImageSubresourceRange subresourceRange{};
        VulkanSDKobjects::ImageSubresourceRangeInit(subresourceRange, VK_IMAGE_ASPECT_COLOR_BIT);

        vkCmdClearColorImage(commandBuffer, m_colorAttachmentImage.image, VK_IMAGE_LAYOUT_GENERAL, 
        &clearColorValue, 1, &subresourceRange);
    }

    void VulkanRenderer::DrawGeometry(const VkCommandBuffer& commandBuffer)
    {
        GPUSceneData* pSceneData = reinterpret_cast<GPUSceneData*>(m_frameToolList[currentFrame].sceneDataBuffer.
        allocation->GetMappedData());
        *pSceneData = m_globalSceneData;

        VkDescriptorSet sceneDataDescriptorSet;
        m_frameToolList[currentFrame].descriptorAllocator.AllocateDescriptorSet(m_device, sceneDataDescriptorSet, 
        m_globalSceneDataDescriptorSetLayout);

        VkDescriptorBufferInfo sceneDataDescriptorBufferInfo{};
        sceneDataDescriptorBufferInfo.buffer = m_frameToolList[currentFrame].sceneDataBuffer.buffer;
        sceneDataDescriptorBufferInfo.offset = 0;
        sceneDataDescriptorBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet sceneDataDescriptorSetWrite{};
        sceneDataDescriptorSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sceneDataDescriptorSetWrite.descriptorCount = 1;
        sceneDataDescriptorSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sceneDataDescriptorSetWrite.dstSet = sceneDataDescriptorSet;
        sceneDataDescriptorSetWrite.dstBinding = 0;
        sceneDataDescriptorSetWrite.pBufferInfo = &sceneDataDescriptorBufferInfo;

        vkUpdateDescriptorSets(m_device, 1, &sceneDataDescriptorSetWrite, 0, nullptr);



        //Specify rendering attachments and start rendering
        VkRenderingAttachmentInfo colorAttachmentRenderingInfo{};
        VulkanSDKobjects::ColorRenderingAttachmentInfoInit(colorAttachmentRenderingInfo, 
        m_colorAttachmentImage.imageView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo depthAttachmentInfo{};
        VulkanSDKobjects::DepthRenderingAttachmentInfoInit(depthAttachmentInfo, m_depthAttachmentImage.imageView, 
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderingInfo{};
        VulkanSDKobjects::RenderingInfoInit(renderingInfo, &colorAttachmentRenderingInfo, m_drawExtent, &depthAttachmentInfo, 
        nullptr);
        vkCmdBeginRendering(commandBuffer, &renderingInfo);

        //Bind the pipeline that will be used for this surface
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_placeholderMaterial.pPipeline->graphicsPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
        m_placeholderMaterialData.opaquePipeline.pipelineLayout, 0, 1, &sceneDataDescriptorSet, 0, nullptr);

        //Since this pipeline has a dynamic viewport and scissor, it has to be set at draw time
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(m_drawExtent.width);
        viewport.height = static_cast<float>(m_drawExtent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = m_drawExtent.width;
        scissor.extent.height = m_drawExtent.height;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        for(size_t i = 0; i < m_mainDrawContext.opaqueObjects.size(); ++i)
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_mainDrawContext.opaqueObjects[i].pMaterial->pPipeline->graphicsPipeline);

            vkCmdBindIndexBuffer(commandBuffer, *(m_mainDrawContext.opaqueObjects[i].pIndexBuffer), 0, VK_INDEX_TYPE_UINT32);

            GPUPushConstant pushConstants;
            pushConstants.vertexBuffer = m_mainDrawContext.opaqueObjects[i].vertexBufferAddress;
            pushConstants.worldMatrix = m_mainDrawContext.opaqueObjects[i].transform;
            vkCmdPushConstants(commandBuffer, m_mainDrawContext.opaqueObjects[i].pMaterial->pPipeline->pipelineLayout, 
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUPushConstant), &pushConstants);

            vkCmdDrawIndexed(commandBuffer, m_mainDrawContext.opaqueObjects[i].indexCount, 1, 
            m_mainDrawContext.opaqueObjects[i].firstIndex, 0, 0);
        }

        vkCmdEndRendering(commandBuffer);
    }

    void VulkanRenderer::ChangeImageLayout(const VkCommandBuffer& commandBuffer, VkImage& image, 
    VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkImageMemoryBarrier2 imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.oldLayout = oldLayout;
        imageMemoryBarrier.newLayout = newLayout;

        imageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        VkImageAspectFlags aspectMask;
        (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT : 
        aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageSubresourceRange subresource{};
        VulkanSDKobjects::ImageSubresourceRangeInit(subresource, aspectMask);
        imageMemoryBarrier.subresourceRange = subresource;

        VkDependencyInfo dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &imageMemoryBarrier;

        vkCmdPipelineBarrier2(commandBuffer, &dependency);
    }

    void VulkanRenderer::CopyImageToImage(const VkCommandBuffer& commandBuffer, VkImage& srcImage, 
    VkImage& dstImage, VkImageLayout srcImageLayout, VkImageLayout dstImageLayout, VkExtent2D srcImageSize, 
    VkExtent2D dstImageSize)
    {
        VkImageBlit2 blit{};
        blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

        blit.srcOffsets[1].x = srcImageSize.width;
        blit.srcOffsets[1].y = srcImageSize.height;
        blit.srcOffsets[1].z = 1;

        blit.dstOffsets[1].x = dstImageSize.width;
        blit.dstOffsets[1].y = dstImageSize.height;
        blit.dstOffsets[1].z = 1;

        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	    blit.srcSubresource.baseArrayLayer = 0;
	    blit.srcSubresource.layerCount = 1;
	    blit.srcSubresource.mipLevel = 0;

	    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	    blit.dstSubresource.baseArrayLayer = 0;
	    blit.dstSubresource.layerCount = 1;
	    blit.dstSubresource.mipLevel = 0;

        VkBlitImageInfo2 blitInfo{};
        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitInfo.srcImage = srcImage;
        blitInfo.dstImage = dstImage;
        blitInfo.srcImageLayout = srcImageLayout;
        blitInfo.dstImageLayout = dstImageLayout;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blit;
        blitInfo.filter = VK_FILTER_LINEAR;

        vkCmdBlitImage2(commandBuffer, &blitInfo);
    }




    void VulkanRenderer::CleanupResources()
    {
        vkDeviceWaitIdle(m_device);

        for(size_t i = 0; i < m_assets.size(); ++i)
        {
            m_assets[i].meshBuffers.CleanupResources(m_device, m_allocator);
        }

        m_placeholderMaterialData.CleanupResources(m_device);
        vkDestroyPipeline(m_device, m_placeholderPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_placeholderPipelineLayout, nullptr);

        CleanupImages();

        for(size_t i = 0; i < BLITZEN_MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_frameToolList[i].CleanupResources(m_device, m_allocator);
        }

        m_instantSubmit.CleanupResources(m_device);

        CleanupVulkanBootstrapObjects();
    }

    void MaterialData::CleanupResources(const VkDevice& device)
    {
        vkDestroyPipeline(device, opaquePipeline.graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, opaquePipeline.pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
    }

    void VulkanRenderer::CleanupImages()
    {
        m_colorAttachmentImage.CleanupResources(m_device, m_allocator);
        m_depthAttachmentImage.CleanupResources(m_device, m_allocator);
    }

    void DescriptorAllocator::CleanupResources(const VkDevice& device)
    {
        for(uint32_t i = 0; i < readyPools.size(); ++i)
        {
            vkDestroyDescriptorPool(device, readyPools[i], nullptr);
        }

        for(size_t i = 0; i < fullPools.size(); ++i)
        {
            vkDestroyDescriptorPool(device, fullPools[i], nullptr);
        }
    }

    void FrameTools::CleanupResources(const VkDevice& device, const VmaAllocator& allocator)
    {
        vkDestroyCommandPool(device , renderCommandPool, nullptr);

        vkDestroyFence(device, inFlightFence, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

        descriptorAllocator.CleanupResources(device);

        vmaDestroyBuffer(allocator, sceneDataBuffer.buffer, sceneDataBuffer.allocation);
    }

    void OneTimeCommands::CleanupResources(const VkDevice& device)
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }

    void VulkanAllocatedImage::CleanupResources(const VkDevice& device, const VmaAllocator& allocator)
    {
        vkDestroyImageView(device, imageView, nullptr);
        vmaDestroyImage(allocator, image, allocation);
    }

    void VulkanAllocatedBuffer::CleanupResources(const VkDevice& device, const VmaAllocator& allocator)
    {
        vmaDestroyBuffer(allocator, buffer, allocation);
    }

    void VulkanGPUMeshBuffers::CleanupResources(const VkDevice& device, const VmaAllocator& allocator)
    {
        vertexBuffer.CleanupResources(device, allocator);
        indexBuffer.CleanupResources(device, allocator);
    }

    void VulkanRenderer::CleanupVulkanBootstrapObjects()
    {
        //Since the swapchain data was also created with vkBootstrap, it will be destroyed here
        CleanupSwapchainData();
        //The allocator must be destroyed late, so that its allocation can be freed first
        vmaDestroyAllocator(m_allocator);
        //The device is one of the last objects to be destroyed since it is called for all other object destructors
        vkDestroyDevice(m_device, nullptr);
        //The surface needs to be destroyed before the instance
        vkDestroySurfaceKHR(m_bootstrapObjects.vulkanInstance, m_bootstrapObjects.windowSurface, nullptr);
        //The debug messenger can be destroyed right before the instance, it has no other dependencies
        vkb::destroy_debug_utils_messenger(m_bootstrapObjects.vulkanInstance, m_bootstrapObjects.debugMessenger, nullptr);
        //The Vulkan Instance will be destroyed last
        vkDestroyInstance(m_bootstrapObjects.vulkanInstance, nullptr);
    }

    void VulkanRenderer::CleanupSwapchainData()
    {
        //Save the image views to a reference to the original array, so that I don't have to write the other thing 3 times
        std::vector<VkImageView>& swapchainImageViews = m_bootstrapObjects.swapchainData.swapchainImageViews;
        //Destroy all swapchain image views
        for(size_t i = 0; i < swapchainImageViews.size(); ++i)
        {
            vkDestroyImageView(m_device, swapchainImageViews[i], nullptr);
        }
        //Now that the image views have been destroyed, the swapchain will also be destroyed which will also cleanup its images
        vkDestroySwapchainKHR(m_device, m_bootstrapObjects.swapchainData.swapchain, nullptr);
    }
}
