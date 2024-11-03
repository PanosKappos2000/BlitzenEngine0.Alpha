#include "mainEngine.h"

namespace BlitzenEngine
{
    MainEngine::MainEngine()
    {
        //The window is created first
        CreateWindow();

        /*-----------------------------------------------------------------
        The glfw callback functions will need to access the controller 
        so that the appropriate function for each event can be called
        -------------------------------------------------------------------*/
        m_windowData.pController = &m_mainController;

        //Setting up some default inputs
        m_mainController.SetKeyPressFunction(GLFW_KEY_ESCAPE, GLFW_PRESS, [&]() {
            m_windowData.bEngineShouldTerminate = true;
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_W, GLFW_PRESS, [&](){
            m_mainCamera.MoveCamera(glm::vec3(0.0f, 0.0f, -1.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_W, GLFW_REPEAT, [&](){
            m_mainCamera.MoveCamera(glm::vec3(0.0f, 0.0f, -1.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_W, GLFW_RELEASE, [&](){});
        m_mainController.SetKeyPressFunction(GLFW_KEY_S, GLFW_PRESS, [&](){
            m_mainCamera.MoveCamera(glm::vec3(0.0f, 0.0f, 1.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_S, GLFW_REPEAT, [&](){
            m_mainCamera.MoveCamera(glm::vec3(0.0f, 0.0f, 1.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_S, GLFW_RELEASE, [&](){});
        m_mainController.SetKeyPressFunction(GLFW_KEY_A, GLFW_PRESS, [&](){
            m_mainCamera.MoveCamera(glm::vec3(-1.0f, 0.0f, 0.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_A, GLFW_REPEAT, [&](){
            m_mainCamera.MoveCamera(glm::vec3(-1.0f, 0.0f, 0.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_A, GLFW_RELEASE, [&](){});
        m_mainController.SetKeyPressFunction(GLFW_KEY_D, GLFW_PRESS, [&](){
            m_mainCamera.MoveCamera(glm::vec3(1.0f, 0.0f, 0.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_D, GLFW_REPEAT, [&](){
            m_mainCamera.MoveCamera(glm::vec3(1.0f, 0.0f, 0.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_D, GLFW_RELEASE, [&](){});
        m_mainController.SetCursorFunctionPointer([&](float x, float y) {
            m_mainCamera.MoveCamera(glm::vec3(0.f, 0.f, 0.f), x, y);
        });

        //Initialize the renderer, only Vulkan is supported for now
        m_vulkan.Init(&m_windowData);
    	/*---------------------------------------------------------------------------------------
    	Declaring two vectors, one for the vertex data and one for the index data
    	The LoadMeshAsset function will go through all the meshes that need to be loaded
    	It will the indices and vertices and give the necessary data to acces them to the objects
    	Then vulkan will allocate two big buffers one for the vertices and one for the indices
    	-----------------------------------------------------------------------------------------*/
    	std::vector<BlitzenRendering::VulkanVertex> vertices;
    	std::vector<uint32_t> indices;
        LoadMeshAsset("BlitzenEngine/Assets/basicmesh.glb", vertices, indices, &m_vulkan);
    	m_vulkan.LoadMeshBuffers(vertices, indices);

        m_vulkan.InitPlaceholderData();

        m_mainCamera.Init(&(m_vulkan.GetGlobalSceneData().viewMatrix), &m_deltaTime);
    }

    void MainEngine::Run()
    {
        std::cout << "Blitzen Engine 0.Alpha Booting\n";

        //Events will be activated right before the main loop, so that no undefined behavior occurs during startup
        InitEvents();

        //Because the setup of the engine takes time, totalRunTime needs to be set before the loop to protect deltaTime from high values
        m_totalRunTime = glfwGetTime();

        while(!(m_windowData.bEngineShouldTerminate))
        {
            float newRunTime = static_cast<float>(glfwGetTime());
            m_deltaTime = newRunTime - m_totalRunTime;
            m_totalRunTime = newRunTime;

            glfwPollEvents();
            m_vulkan.DrawFrame();
        }
    }

    MainEngine::~MainEngine()
    {
        std::cout << "Bliten Engine 0.Alpha Termination\n";

        m_vulkan.CleanupResources();

        //Once all engine systems have been stopped, the window should be destroyed and glfw should terminate
        glfwDestroyWindow(m_windowData.pWindow);

        glfwTerminate();
    }

    void MainEngine::CreateWindow()
    {
        //The engine will use glfw as its windowing system
        int glfwResult = glfwInit();
        if(!glfwResult)
        {
            std::cout << "glfwInitFailed";
        }

        //Telling glfw to not create an opengl context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        //Creating the window and storing its data to the WindowData struct
        m_windowData.pWindow = glfwCreateWindow(m_windowData.windowWidth, m_windowData.windowHeight, 
        m_windowData.windowTitle, nullptr, nullptr);
    }

    void MainEngine::InitEvents()
    {
        //This will make it so that glfw can access some important data in the callback functions that will be set
        glfwSetWindowUserPointer(m_windowData.pWindow, &m_windowData);

        //Setting the function that gets called when user input tells the window to close
        glfwSetWindowCloseCallback(m_windowData.pWindow, glfwWindowCloseCallback);

        //Setting the function that gets called when user input asks for resize
        glfwSetWindowSizeCallback(m_windowData.pWindow, glfwWindowSizeCallback);

        glfwSetKeyCallback(m_windowData.pWindow, glfwKeyCallback);

        //Setting the position of the cursor and its callback
        glfwGetCursorPos(m_windowData.pWindow, &(m_windowData.currentCursorX), &(m_windowData.currentCursorY));
        glfwSetCursorPosCallback(m_windowData.pWindow, glfwCursorCallback);
    }
}
