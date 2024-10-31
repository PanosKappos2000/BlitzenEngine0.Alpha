#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct WindowData
    {
        GLFWwindow* pWindow;
        int windowWidth = 850;
        int windowHeight = 620;
        const char* windowTitle = "Blitzen 0Alpha";
        bool bEngineShouldTerminate = false;
        bool bResizeRequested = false;
        bool bPauseRendering = false;
    };

namespace BlitzenEngine
{
    void glfwWindowCloseCallback(GLFWwindow* pWindow);

    void glfwWindowSizeCallback(GLFWwindow* pWindow, int width, int height);
}