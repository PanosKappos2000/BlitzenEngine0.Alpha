#include "glfwCallbacks.h"
#include "mainEngine.h"

namespace BlitzenEngine
{
    void glfwWindowCloseCallback(GLFWwindow* pWindow)
    {
        //Retrieve the window user pointer
        WindowData* pData = reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(pWindow));

        //Signal to the engine that it should terminate
        pData->bEngineShouldTerminate = true;
    }

    void glfwWindowSizeCallback(GLFWwindow* pWindow, int width, int height)
    {
        WindowData* pData = reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(pWindow));

        if (width == 0 || height == 0)
        {
            pData->bPauseRendering = true;
            return;
        }

        pData->bPauseRendering = false;
        pData->windowWidth = width;
        pData->windowHeight = height;
        pData->bResizeRequested = true;
    }
}