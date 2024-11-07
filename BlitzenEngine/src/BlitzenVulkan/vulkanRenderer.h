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

        //Explicit constructor, so that the Engine can call it when other important tools have been initialized
        void Init(WindowData* pWindowData);

        //This is used for the beginning stages of this engine, so that tools that don't work yet don't slow it down
        void InitPlaceholderData();

        void InitMeshNodes();

        //Takes the vertices and indices of an object and writes them to a GPU only SSBO with pointer access
        void LoadMeshBuffers(std::vector<VulkanVertex>& vertices, 
        std::vector<uint32_t>& indices);

        //Passes material values and resources to the descriptor set, so that the descriptor set for surfaces with this material
        void WriteMaterial(MaterialInstance& instance, VkDevice device, MaterialPass pass, 
        MaterialResources& resources);

        //Allocates an image to be used as a color attachment or depth attachment 
        void AllocateImage(VulkanAllocatedImage& imageToAllocate, VkExtent3D imageExtent, VkFormat imageFormat, 
        VkImageUsageFlags imageUsage, bool bMipmapped = false);

        //Allocates an image and copies data to it, for assets like textures
        void AllocateImage(void* data, VulkanAllocatedImage& imageToAllocate, VkExtent3D imageExtent, VkFormat imageFormat, 
        VkImageUsageFlags imageUsage, bool bMipmapped = false);

        //Allocates a buffer using VMA
        void AllocateBuffer(VulkanAllocatedBuffer& bufferToAllocate, VkDeviceSize bufferSize, 
        VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);

        //This function is responsible for calling the right operation to draw all objects in a frame
        void DrawFrame();

        //Explicit destructor so that the engine can cleanup Vulkan at the right time
        void CleanupResources();

        //Setting the constructor to default and destroy copy operators
        VulkanRenderer();
        VulkanRenderer operator = (VulkanRenderer& vulkan) = delete;
    private:

        /*-----------------------
        Init helper functions
        ------------------------*/

        //Uses VkBootstrap to create a VkInstance and returns the bootstrap instance handler for the next functions
        vkb::Instance BootstrapCreateInstance();

        //Selects the GPU that Vulkan will interface with and creates the device based on it
        void BootstrapSelectGPUAndCreateDevice(const vkb::Instance& vkbInstance);

        //Initializes the allocator that will be used for buffer and image memory allocations
        void InitAllocator();

        void BootstrapCreateSwapchain();

        //Initalizes command buffers and sync objects in each frame tools struct
        void InitFrameTools();

        void InitPlaceholderMaterial();


        /*---------------------------
        Draw loop helper functions
        -----------------------------*/

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


        /*----------------------------
        Cleanup helper functions
        -----------------------------*/

        void CleanupImages();

        void CleanupSwapchainData();
        
        void CleanupVulkanBootstrapObjects();

    public:

        //Will be constantly called to interface with the GPU and create or destroy other Vulkan objects
        VkDevice m_device{VK_NULL_HANDLE};

        VmaAllocator m_allocator{VK_NULL_HANDLE};

        //Used to build all graphics pipelines that might need to be bound by Vulkan each time a frame is drawn
        VulkanGraphicsPipelineBuilder m_graphicsPipelineBuilder;

        //Keeps track of the object assets that vulkan will have to access while drawing
        std::vector<VulkanMeshAsset> m_assets;

        //Holds the mesh assets, textures, materials and nodes of a scene loaded from a gltf file
        std::unordered_map<std::string, LoadedGLTFScene> m_loadedScenes;
        //Holds placeholder material data for colors, textures and most importantly a universal descriptor layout
        MaterialData m_placeholderMaterialData;
        //Some placeholder/default data for textures, used while renderer implementations are not fully realized
        VulkanAllocatedImage m_placeholderWhiteTextureImage;
        VulkanAllocatedImage m_placeholderBlackTextureImage;
        VulkanAllocatedImage m_placeholderGreyTextureImage;
        VulkanAllocatedImage m_placeholderErrorTextureImage;
        VkSampler m_placeholderLinearSampler;
        VkSampler m_placeholderNearestSampler;

        //The global scene data might need to be manipulated for some of the game logic
        inline GPUSceneData& GetGlobalSceneData() {return m_globalSceneData;}
    
    private:

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

        GPUSceneData m_globalSceneData;
        VkDescriptorSetLayout m_globalSceneDataDescriptorSetLayout{VK_NULL_HANDLE};

        //Holds the unified index and vertex buffer for all objects
	    VulkanGPUMeshBuffers m_meshBuffers;

        DrawContext m_mainDrawContext;
        std::unordered_map<std::string, Node> m_nodeTable;
    };
}
