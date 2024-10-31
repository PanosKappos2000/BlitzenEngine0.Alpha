#include "mainEngine.h"

namespace BlitzenEngine
{
    MainEngine::MainEngine()
    {
        //The window is created first
        CreateWindow();

        //Since the window ensures that glfw is initialized, events will now be set
        InitEvents();

        //Initialize the renderer, only Vulkan is supported for now
        m_vulkan.Init(&m_windowData);

        LoadMeshAsset("BlitzenEngine/Assets/basicmesh.glb", &m_vulkan);

        m_vulkan.InitPlaceholderData();
    }

    void MainEngine::Run()
    {
        std::cout << "Blitzen Engine 0.Alpha Booting\n";

        while(!(m_windowData.bEngineShouldTerminate))
        {
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
    }
}