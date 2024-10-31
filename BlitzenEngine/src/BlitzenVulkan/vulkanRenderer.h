#pragma once

#include <array>
#include <vector>
#include <deque>

//Includes glfw and Vulkan while also including the WindowData structure
#include "Inputs/glfwCallbacks.h"

//Includes the vulkan memory allocation library that will be used for Blitzen's renderer
#include "vma/vk_mem_alloc.h"

#include "VkBootstrap.h"

#include "vulkanPipelines.h"
#include "vulkanRenderData.h"


namespace BlitzenRendering
{
    //When Vuklan is busy drawing one frame, the cpu should be allowed to start processing the next one
    #define BLITZEN_MAX_FRAMES_IN_FLIGHT 2

    //Holds the swapchain handle and all relevant data
    struct SwapchainData
    {
        VkSwapchainKHR swapchain{VK_NULL_HANDLE};
        VkExtent2D swapchainExtent{0};
        VkFormat imageFormat{VK_FORMAT_UNDEFINED};
        std::vector<VkImage> swapchainImages{0};
        std::vector<VkImageView> swapchainImageViews{0};
    };

    /*----------------------------------------------------------------------------------------
    Holds all objects initialized by VkBootstrap. These objects will not be called upon often
    during the application but are crucial for Vulkan initialization.
    The only exception is the vulkan logical device (VkDevice) which is not included in this
    so that the engine can access it faster
    -----------------------------------------------------------------------------------------*/
    struct VulkanBootstrapObjects
    {
        VkInstance vulkanInstance{VK_NULL_HANDLE};
        //This should only be enabled on debug mode, I will add that functionality later
        const bool bEnableValidationLayers = true;
        VkDebugUtilsMessengerEXT debugMessenger;
        VkPhysicalDevice chosenGPU;
        VkSurfaceKHR windowSurface;
        SwapchainData swapchainData;
    };

    //Holds the different queue that will be for different types of commands and their index
    struct VulkanQueues
    {
        uint32_t graphicsQueueFamilyIndex;
        VkQueue graphicsQueue{VK_NULL_HANDLE};

        uint32_t presentQueueFamilyIndex;
        VkQueue presentQueue{VK_NULL_HANDLE};
    };

    /*---------------------------------------------------------------------------------------------
    This struct keeps track of all descriptor pools used to allocate descriptor sets. 
    When the already existing descriptor pools are not sufficient for the descriptors, 
    it allocates a new one and saves it so that more are available each frame
    -----------------------------------------------------------------------------------------------*/
    struct DescriptorAllocator
    {
    public:
        void Init(const VkDevice& device);

        void AllocateDescriptorSet(const VkDevice& device, VkDescriptorSet& descriptorSetToAllocate,
         VkDescriptorSetLayout& layout);

        void ResetPools(const VkDevice& device);

        void CleanupResources(const VkDevice& device);
    private:
        void CreateDescriptorPool(const VkDevice& device);
        size_t GetDescriptorPoolIndex(const VkDevice& device);

        std::vector<VkDescriptorPool> readyPools;
        std::vector<VkDescriptorPool> fullPools;
    };

    struct DescriptorWriter 
    {
        std::deque<VkDescriptorImageInfo> imageInfos;
        std::deque<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkWriteDescriptorSet> writes;

        void WriteImage(int binding, VkImageView& image, VkSampler& sampler, VkImageLayout layout, VkDescriptorType type);
        void WriteBuffer(int binding, VkBuffer& buffer, size_t size, size_t offset, VkDescriptorType type); 

        void Clear();
        void UpdateSet(const VkDevice& device, VkDescriptorSet& set);
    };


    /*----------------------------------------------------------------------------------------------
    Holds the objects that each frame relies upond for commands and command synchronization
    -----------------------------------------------------------------------------------------------*/
    struct FrameTools
    {
        VkCommandPool renderCommandPool;
        //Will be used throughout the render loop to record all commands
        VkCommandBuffer renderCommandBuffer;

        //Will be signaled when the previous frame that was using this fence finishes presentation
        VkFence inFlightFence;
        //Will be signaled when the swapchain has given an image to Vulkan
        VkSemaphore imageAvailableSemaphore;
        //Will be signaled when the renderCommandBuffer has been submitted to the graphics queue
        VkSemaphore renderFinishedSemaphore;

        DescriptorAllocator descriptorAllocator;

        VulkanAllocatedBuffer sceneDataBuffer;

        void CleanupResources(const VkDevice& device, const VmaAllocator& allocator);
    };

    //This struct is used for one time commands outside of draw frame
    struct OneTimeCommands
    {
        VkCommandPool commandPool{VK_NULL_HANDLE};
        VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
        VkQueue* submitQueue;
        uint32_t submitQueueFamilyIndex;

        void Init(const VkDevice& m_device, uint32_t queueFamilyIndex, VkQueue* queue);

        void StartRecording();

        void EndRecordingAndSubmit();

        void CleanupResources(const VkDevice& m_device);
    };

    class VulkanRenderer
    {
    public:
        /*-------------------------------------------------------------------------
        Since this is one of the biggest tools of the engine, it has an explicit
        initialization function, so that the engine can call it at the right time
        ----------------------------------------------------------------------------*/
        void Init(WindowData* pWindowData);

        //This is used for the beginning stages of this engine, so that tools that don't work yet don't slow it down
        void InitPlaceholderData();

        void LoadMeshBuffers(VulkanGPUMeshBuffers& meshBuffers, std::vector<VulkanVertex>& vertices, 
        std::vector<uint32_t>& indices);

        void WriteMaterial(MaterialInstance& instance, VkDevice device, MaterialPass pass, 
        MaterialResources& resources);

        /*---------------------------------------------------------------------------
        This function is called when all engine objects are ready to be renderered.
        It requests a swapchain image, records a command buffer for all the draw
        commands that the engine needs and submits to a queue and presents to the 
        swapchain
        ----------------------------------------------------------------------------*/
        void DrawFrame();

        //The destructor will be explicit so that the main engine can destroy it at the correct time
        void CleanupResources();

        //Setting the constructor to default and destroy copy operators
        VulkanRenderer();
        VulkanRenderer operator = (VulkanRenderer& vulkan) = delete;
    private:

        //Uses VkBootstrap to create a VkInstance and returns the bootstrap instance handler for the next functions
        vkb::Instance BootstrapCreateInstance();

        //Selects the GPU that Vulkan will interface with and creates the device based on it
        void BootstrapSelectGPUAndCreateDevice(const vkb::Instance& vkbInstance);

        //Initializes the allocator that will be used for buffer and image memory allocations
        void InitAllocator();

        void BootstrapCreateSwapchain();



        //Initalizes command buffers and sync objects in each frame tools struct
        void InitFrameTools();



        //Allocates an image 
        void AllocateImage(VulkanAllocatedImage& imageToAllocate, VkExtent3D imageExtent, VkFormat imageFormat, 
        VkImageUsageFlags imageUsage, bool bMipmapped = false);



        void AllocateBuffer(VulkanAllocatedBuffer& bufferToAllocate, VkDeviceSize bufferSize, 
        VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);



        void InitPlaceholderMaterial();



        //Updates global scene data and adds the objects than need to be draw to the draw context
        void UpdateScene();
        /*-----------------------------------------------------------------------
        In draw frame, after a swapchain image has been acquired, 
        this is called so that all functions that record commands can be called
        -------------------------------------------------------------------------*/
        void StartRecordingFrameCommands(const VkCommandBuffer& commandBuffer, uint32_t swapchainImageIndex);

        //Changes an image's layout so that it can target specific operations
        void ChangeImageLayout(const VkCommandBuffer& commandBuffer, VkImage& image, VkImageLayout oldLayout, 
        VkImageLayout newLayout);

        //Draws the background of the window. In the future this will use a compute shader
        void DrawBackground(const VkCommandBuffer& commandBuffer);

        void DrawGeometry(const VkCommandBuffer& commandBuffer);

        //Copies the contents of one image to the other
        void CopyImageToImage(const VkCommandBuffer& commandBuffer, VkImage& srcImage, VkImage& dstImage, 
        VkImageLayout srcImageLayout, VkImageLayout dstImageLayout, VkExtent2D srcImageSize, VkExtent2D dstImageSize);



        /*---------------------------------------------------------------------------------------------
        Each of the functions below will be called by CleanupResources in the right order to destroy 
        all the resources that was be used by Vulkan during the engine's lifetime
        -----------------------------------------------------------------------------------------------*/
        void CleanupImages();

        void CleanupSwapchainData();
        
        void CleanupVulkanBootstrapObjects();

    public:

        //Used to build all graphics pipelines that might need to be bound by Vulkan each time a frame is drawn
        VulkanGraphicsPipelineBuilder m_graphicsPipelineBuilder;

        //This will be used for each allocated descriptor set that needs to be updated
        DescriptorWriter m_descriptorWriter;

        //Keeps track of the object assets that vulkan will have to access while drawing
        std::vector<VulkanMeshAsset> m_assets;
    
    private:

        //Will be constantly called to interface with the GPU and create or destroy other Vulkan objects
        VkDevice m_device{VK_NULL_HANDLE};

        VmaAllocator m_allocator{VK_NULL_HANDLE};

        //Since the renderer allows for 2 frames to be flight, this shows the frame that the CPU is processing
        uint8_t currentFrame = 0;

        //Vulkan will need to interact with the glfw window for some functionality and change some of its aspects
        WindowData* m_pWindowData;

        //The fisrt objects to be initialized along with the VkDevice object
        VulkanBootstrapObjects m_bootstrapObjects;

        //Holds the graphics and present queue and their families
        VulkanQueues m_queues;

        /*-----------------------------------------------------------------------------------------------
        These are tools that are heavily relied upon during the render loop and should have a different
        instance for each frame in flight
        ------------------------------------------------------------------------------------------------*/
        std::array<FrameTools, BLITZEN_MAX_FRAMES_IN_FLIGHT> m_frameToolList;

        //This is mostly used for copy commands during initialization
        OneTimeCommands m_instantSubmit;

        //Rendering attachments that will be used in the draw loop
        VulkanAllocatedImage m_colorAttachmentImage;
        VulkanAllocatedImage m_depthAttachmentImage;

        //Holds the extent in which the renderer can draw. For now it will always be the same as the window extent
        VkExtent2D m_drawExtent{0};

        VkPipeline m_placeholderPipeline{VK_NULL_HANDLE};
        VkPipelineLayout m_placeholderPipelineLayout{VK_NULL_HANDLE};
        VulkanGPUMeshBuffers m_placeholderMesh;
        MaterialInstance m_placeholderMaterial;
        MaterialData m_placeholderMaterialData;

        GPUSceneData m_globalSceneData;
        VkDescriptorSetLayout m_globalSceneDataDescriptorSetLayout{VK_NULL_HANDLE};

        DrawContext m_mainDrawContext;
        std::unordered_map<std::string, Node> m_nodeTable;
    };
}