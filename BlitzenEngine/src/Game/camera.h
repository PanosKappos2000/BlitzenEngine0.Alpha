#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace BlitzenEngine
{
    class Camera
    {
    public:
        void Init(glm::mat4* pMatrix, const float* pDeltaTime);

        void MoveCamera(const glm::vec3& velocity, float yawMovement, float pitchMovement);

    private:
        glm::mat4* m_viewMatrix;
        const float* m_pDeltaTime;

        glm::vec3 m_position = glm::vec3(0.f, 0.f, 5.0f);

        float pitch = 0.f;
        float yaw = 0.f;

        float m_speed = 5.0f;
        float m_sensitivity = 10.f;
    };
}