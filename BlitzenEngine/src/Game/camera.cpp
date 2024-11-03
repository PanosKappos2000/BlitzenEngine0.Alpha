#include "camera.h"

namespace BlitzenEngine
{
    void Camera::Init(glm::mat4* pMatrix, const float* pDelta)
    {
        m_viewMatrix = pMatrix;
        m_pDeltaTime = pDelta;

        MoveCamera(glm::vec3(0.f, 0.f, 0.f), 0.f, 0.f);
    }

    void Camera::MoveCamera(const glm::vec3& velocity, float yawMovement, float pitchMovement)
    {
        /*
        Since the cursor is active inside and outside the window,
        the controller might pass undesirable values.
        These if statements guard against that
        */
        if (yawMovement < 100.f && yawMovement > -100.f)
            yaw += (yawMovement * m_sensitivity * (*m_pDeltaTime)) / 100.f;
        if (pitchMovement < 100.f && pitchMovement > -100.0f)
            pitch -= (pitchMovement * m_sensitivity * (*m_pDeltaTime)) / 100.f;

        //Find the camera's current rotation first
        glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
        glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f });
        glm::mat4 cameraRotation = glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);

        //Translate the position of the camera on the x axis based on the orientation given by the input
        m_position += glm::vec3(cameraRotation * glm::vec4(velocity * m_speed * (*m_pDeltaTime), 0.f));
        glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), m_position);

        //Change the view matrix by moving every single item relative to the camera's translation
        *m_viewMatrix = glm::inverse(cameraTranslation * cameraRotation);
    }
}