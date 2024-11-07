#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace BlitzenEngine
{
    class Camera
    {
    public:
        //Explicit init function as the camera needs some pointers before it can interact with other engine tools
        void Init(glm::mat4* pView, const float* pDelta);

        void MoveCamera(const glm::vec3& velocity, float yawMovement, float pitchMovement);

    private:
        //The camera has direct access to the view matrix memory, so that it can change its value without interacting with the renderer
        glm::mat4* m_pViewMatrix;
        //The camera has read access to the engine's delta time, so that its movement speed is not FPS dependent
        const float* m_pDeltaTime;

        //Based on this position the view matrix will be generated which will translate all rendered objects based on the camera
        glm::vec3 m_position = glm::vec3(0.0f, 0.0f, 5.0f);

        float pitch = 0.f;
        float yaw = 0.f;

        float m_sensitivity = 10.f;
        float m_speed = 35.f;
    };
}