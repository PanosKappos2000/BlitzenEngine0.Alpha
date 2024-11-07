#include "Camera.h"

namespace BlitzenEngine
{
    void Camera::Init(glm::mat4* pView, const float* pDelta)
    {
        m_pDeltaTime = pDelta;
        m_pViewMatrix = pView;
        //Change the renderer's view matrix so that the Camera starts at the set position
        MoveCamera(glm::vec3(0.f), 0.f, 0.f);
    }

    void Camera::MoveCamera(const glm::vec3& velocity, float yawMovement, float pitchMovement)
    {
        /*
        Since the cursor is active inside and outside the window,
        the controller might pass undesirable values.
        These if statements guard against that
        */
        if(yawMovement < 100.f && yawMovement > -100.f)
            yaw += (yawMovement * m_sensitivity * (*m_pDeltaTime)) /100.f;
        if(pitchMovement < 100.f && pitchMovement > -100.0f)
            pitch -= (pitchMovement * m_sensitivity * (*m_pDeltaTime)) / 100.f;

        glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3(1.f, 0.f, 0.f));
        glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3(0.f, -1.f, 0.f));
        glm::mat4 rotation = glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);

        m_position += glm::vec3(rotation * glm::vec4(velocity * (*m_pDeltaTime) *m_speed, 0.f));
        glm::mat4 translation = glm::translate(glm::mat4(1.f), m_position);

        *m_pViewMatrix = glm::inverse(translation * rotation);
    }
}