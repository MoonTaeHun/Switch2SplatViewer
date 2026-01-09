#ifndef SPLATTINGWIDGET_H
#define SPLATTINGWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <vector>
#include "Camera.h"
#include "GaussianData.h"

class SplattingWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
    Q_OBJECT

public:
    explicit SplattingWidget(QWidget *parent = nullptr);
    ~SplattingWidget();

    // 외부에서 데이터를 넘겨주는 함수
    void loadData(const std::vector<RenderSplat>& splats);

    // UI에서 조절할 설정값 세터(Setter)
    void setGlobalScale(float scale);
    void setAlphaCutoff(float cutoff);

    // 필터링 모드 설정 함수
    void setUpscaleFilter(bool isLinear);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    // 마우스 이벤트 오버라이딩
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // 렌더링 리소스 초기화 함수
    void initShaders();

#if 0
    // initGeometry는 이제 쓰지 않고 loadData에서 처리합니다
    void initGeometry();
#endif

    // 정렬 함수
    void sortSplats(const QMatrix4x4& viewMatrix);

private:
    // 핵심: 오프스크린 렌더링용 FBO
    QOpenGLFramebufferObject *m_fbo = nullptr;
    
    // 내부 렌더링 해상도 (Switch 2 Portable Mode Target: 720p)
    const int INTERNAL_WIDTH = 1280;
    const int INTERNAL_HEIGHT = 720;

    // OpenGL 리소스
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_instanceVbo; // 데이터(위치/색상) 담는 버퍼
    QOpenGLBuffer m_quadVbo;     // 사각형 모양 담는 버퍼

    Camera m_camera;

    // 렌더링할 점의 개수
    int m_splatCount = 0;

    // 원본 데이터를 저장해둘 벡터 (정렬 대상)
    std::vector<RenderSplat> m_splats;

    // 최적화 및 설정 변수들
    bool m_needsSort = false;    // 정렬이 필요한가?
    float m_globalScale = 1.0f;  // 전체 크기 조절
    float m_alphaCutoff = 0.05f; // 투명도 컷오프

    // FPS 측정용
    QElapsedTimer m_fpsTimer;
    int m_frameCount = 0;
    float m_currentFps = 0.0f;

    // 필터링 모드 변수 (기본값 true: 부드럽게)
    bool m_useLinearFilter = true;
};

#endif // SPLATTINGWIDGET_H
