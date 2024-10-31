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
}