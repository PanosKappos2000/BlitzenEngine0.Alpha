#pragma once

#include <vector>
#include <array>
#include <iostream>
#include <fstream>

#include <GLFW/glfw3.h>

#include "Inputs/glfwCallbacks.h"

#include "BlitzenVulkan/vulkanRenderer.h"

#include "AssetLoading/assetLoading.h"

#define INITIAL_WINDOW_WIDTH        850
#define INITIAL_WINDOW_HEIGHT       620


namespace BlitzenEngine
{
    /*-------------------------------------------------------------------------------------
    The main engine class, responsible for holding global data, building all the systems
    and running the main loop
    --------------------------------------------------------------------------------------*/
    class MainEngine
    {
    public:
        MainEngine();

        //This function runs the engine until an even stop the game loop
        void Run();

        ~MainEngine();

        MainEngine(MainEngine& engine) = delete;

        MainEngine operator = (MainEngine& engine) = delete;
    private:

        //Called at the start of engine initialization, it creates the main window
        void CreateWindow();

        //Called after glfw has been initialized to set the callback function for events like window resizing
        void InitEvents();

    private:

        //Holds data about the window that glfw should access when an input callback gets triggered
        WindowData m_windowData;

        //Holds an instance of the class that will handle all Vulkan functionality
        BlitzenRendering::VulkanRenderer m_vulkan;
    };
}