#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Game/controller.h"

struct WindowData
    {
        GLFWwindow* pWindow;
        int windowWidth = 850;
        int windowHeight = 620;
        const char* windowTitle = "Blitzen 0Alpha";
        double currentCursorX;
        double currentCursorY;

	    bool bEngineShouldTerminate = false;
        bool bResizeRequested = false;
	    bool bPauseRendering = false;
        BlitzenEngine::Controller* pMainController;
    };

namespace BlitzenEngine
{
    void glfwWindowCloseCallback(GLFWwindow* pWindow);

    void glfwWindowSizeCallback(GLFWwindow* pWindow, int width, int height);

    void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    void glfwCursorCallback(GLFWwindow* pWindow, double mouseX, double mouseY);
}
