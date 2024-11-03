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

        //If the window gets minimized, the renderer should be paused
        if (width == 0 || height == 0)
        {
            pData->bPauseRendering = true;
            return;
        }

        //If the window simply gets resized, its width and height are updated and the engine is warned about the event
        pData->bPauseRendering = false;
        pData->windowWidth = width;
        pData->windowHeight = height;
        pData->bResizeRequested = true;
    }

    void glfwKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods)
    {
        WindowData* pData = reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(pWindow));

        if (pData->pController->m_KeyFunctionPointers.find(key) != pData->pController->m_KeyFunctionPointers.end())
        {
            pData->pController->m_KeyFunctionPointers[key][action]();
        }
    }

    void glfwCursorCallback(GLFWwindow* pWindow, double mouseX, double mouseY)
    {
        WindowData* pData = reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(pWindow));

        //Save the new cursor pos after retrieving the movement of the cursor
        double xMovement = mouseX - pData->currentCursorX;
        double yMovement = mouseY - pData->currentCursorY;
        pData->currentCursorX = mouseX;
        pData->currentCursorY = mouseY;

        //Call the set function for cursor movement, engine sets it to camera rotation by default
        pData->pController->m_pfnCursor(static_cast<float>(xMovement), static_cast<float>(yMovement));
    }
}