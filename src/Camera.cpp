#include "Camera.h"
#include <QtMath>

Camera::Camera() {}

QMatrix4x4 Camera::getViewMatrix() const
{
    QMatrix4x4 view;

    // 구면 좌표계 변환 (Orbit Camera 계산)
    float yawRad = qDegreesToRadians(m_yaw);
    float pitchRad = qDegreesToRadians(m_pitch);

    QVector3D position;
    position.setX(m_target.x() + m_distance * cos(pitchRad) * sin(yawRad));
    position.setY(m_target.y() + m_distance * sin(pitchRad));
    position.setZ(m_target.z() + m_distance * cos(pitchRad) * cos(yawRad));

    view.lookAt(position, m_target, QVector3D(0, 1, 0));
    return view;
}

QMatrix4x4 Camera::getProjectionMatrix(float aspectRatio) const
{
    QMatrix4x4 projection;
    // FOV 45도, 근평면 0.1, 원평면 100.0
    projection.perspective(45.0f, aspectRatio, 0.1f, 100.0f);
    return projection;
}

void Camera::handleMousePress(QMouseEvent *event)
{
    m_lastPos = event->pos();
}

void Camera::handleMouseMove(QMouseEvent *event)
{
    float dx = event->position().x() - m_lastPos.x();
    float dy = event->position().y() - m_lastPos.y();

    if (event->buttons() & Qt::LeftButton) {
        // 좌클릭: 회전
        m_yaw -= dx * 0.5f;
        m_pitch -= dy * 0.5f;

        // Pitch 제한 (고개 너무 꺾임 방지)
        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;
    }
    else if (event->buttons() & Qt::RightButton) {
        // 우클릭: 패닝 (타겟 이동) - 구현은 나중에, 일단 빈칸
    }

    m_lastPos = event->pos();
}

void Camera::handleWheel(QWheelEvent *event)
{
    // 휠 스크롤: 줌 인/아웃
    float numDegrees = event->angleDelta().y() / 8.0f;
    float numSteps = numDegrees / 15.0f;

    m_distance -= numSteps * 0.2f;
    if (m_distance < 0.1f) m_distance = 0.1f; // 너무 가까워짐 방지
}
