#include "mainEngine.h"

namespace BlitzenEngine
{
    MainEngine::MainEngine()
    {
        //The window is created first
        CreateWindow();

        m_windowData.pMainController = &m_mainController;

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

        m_mainCamera.Init(&(m_vulkan.GetSceneData().viewMatrix), &m_deltaTime);

        m_vulkan.m_loadedScenes["structure"] = BlitzenRendering::LoadedGLTF();
        LoadScene("BlitzenEngine/Assets/Structure.glb", &m_vulkan, m_vulkan.m_loadedScenes["structure"]);
    }

    void MainEngine::Run()
    {
        std::cout << "Blitzen Engine 0.Alpha Booting\n";

        //Setting events here so that no undefined behavior during loading
        InitEvents();

        //Because loading might take time, total time is set here so that delta time does not cause undefined behavior
        m_totalRunTime = static_cast<float>(glfwGetTime());

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

        //Setting the function that gets called when a user presses a key
        glfwSetKeyCallback(m_windowData.pWindow, glfwKeyCallback);

        glfwGetCursorPos(m_windowData.pWindow, &(m_windowData.currentCursorX), &(m_windowData.currentCursorY));
        glfwSetCursorPosCallback(m_windowData.pWindow, glfwCursorCallback);
    }
}