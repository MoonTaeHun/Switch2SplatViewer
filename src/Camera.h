#ifndef CAMERA_H
#define CAMERA_H

#include <QVector3D>
#include <QMatrix4x4>
#include <QMouseEvent>

class Camera
{
public:
    Camera();

    // 뷰 행렬 (카메라 위치/방향)
    QMatrix4x4 getViewMatrix() const;
    // 투영 행렬 (원근감)
    QMatrix4x4 getProjectionMatrix(float aspectRatio) const;

    // 마우스 입력 처리
    void handleMousePress(QMouseEvent *event);
    void handleMouseMove(QMouseEvent *event);
    void handleWheel(QWheelEvent *event);

private:
    // 카메라 상태
    QVector3D m_target = QVector3D(0.0f, 0.0f, 0.0f); // 바라보는 점
    float m_distance = 3.0f; // 타겟과의 거리
    float m_yaw = 0.0f;      // 좌우 회전 각도
    float m_pitch = 0.0f;    // 상하 회전 각도

    // 마우스 조작용 변수
    QPoint m_lastPos;
};

#endif // CAMERA_H
