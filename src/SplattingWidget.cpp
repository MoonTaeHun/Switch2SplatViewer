#include "SplattingWidget.h"
#include <QPainter>
#include <QDebug>

SplattingWidget::SplattingWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // [추가] 이 위젯이 마우스 클릭/휠 이벤트를 받을 수 있도록 설정
    setFocusPolicy(Qt::StrongFocus);

    m_fpsTimer.start(); // 타이머 시작
}

SplattingWidget::~SplattingWidget()
{
    makeCurrent();
    delete m_fbo;
    delete m_program;
    m_instanceVbo.destroy();
    m_quadVbo.destroy();
    m_vao.destroy();
    doneCurrent();
}

// [핵심] 데이터 로드 및 GPU 업로드
void SplattingWidget::loadData(const std::vector<RenderSplat>& splats)
{
    if (splats.empty()) return;

    // 멤버 변수에 복사본 저장
    m_splats = splats;
    m_splatCount = static_cast<int>(m_splats.size());
    m_needsSort = true;

    makeCurrent(); // OpenGL 컨텍스트 활성화

    m_splatCount = static_cast<int>(splats.size());

    m_vao.bind();
    m_instanceVbo.bind();

    // 데이터 업로드
    m_instanceVbo.allocate(splats.data(), m_splatCount * sizeof(RenderSplat));

    int stride = sizeof(RenderSplat);

    // RenderSplat 구조: {x,y,z, r,g,b, opacity, scale[3], rot[4]}

    // [Layout 1] Instance Position (vec3) -> offset 0
    m_program->enableAttributeArray(1);
    m_program->setAttributeBuffer(1, GL_FLOAT, 0, 3, stride);
    glVertexAttribDivisor(1, 1);

    // [Layout 2] Instance Color (vec3) -> offset 12 (3 floats)
    m_program->enableAttributeArray(2);
    m_program->setAttributeBuffer(2, GL_FLOAT, 3 * sizeof(float), 3, stride);
    glVertexAttribDivisor(2, 1);

    // [Layout 3] Instance Opacity (1 float) -> offset 24 (6 floats * 4 bytes)
    m_program->enableAttributeArray(3);
    m_program->setAttributeBuffer(3, GL_FLOAT, 6 * sizeof(float), 1, stride);
    glVertexAttribDivisor(3, 1);

    // [Layout 4] Instance Scale -> Layout 번호 3에서 4로 변경
    // Opacity(1 float) 다음이므로 offset은 28 (7 floats * 4 bytes)
    m_program->enableAttributeArray(4);
    m_program->setAttributeBuffer(4, GL_FLOAT, 7 * sizeof(float), 3, stride);
    glVertexAttribDivisor(4, 1);

    m_instanceVbo.release();
    m_vao.release();
    doneCurrent();
    update();  // 화면 갱신 요청
}

// 설정값 변경 함수
void SplattingWidget::setGlobalScale(float scale) {
    m_globalScale = scale;
    update(); // 화면 갱신
}
void SplattingWidget::setAlphaCutoff(float cutoff) {
    m_alphaCutoff = cutoff;
    update();
}

void SplattingWidget::setUpscaleFilter(bool isLinear) {
    m_useLinearFilter = isLinear;
    update(); // 즉시 다시 그리기
}

void SplattingWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // 1. 셰이더 초기화 (간단한 패스스루 + 빨간색 출력)
    initShaders();

#if 0
    // 2. 지오메트리(삼각형) 초기화
    initGeometry();
#else
    // VAO, VBO 생성
    m_vao.create();
    m_vao.bind();

    // 1. 사각형(Quad) 지오메트리 생성 (Triangle Strip 사용)
    // (-1, -1) ~ (1, 1) 크기의 정사각형
    float quadVertices[] = {
        -1.0f, -1.0f, // 좌하단
        1.0f, -1.0f, // 우하단
        -1.0f,  1.0f, // 좌상단
        1.0f,  1.0f  // 우상단
    };

    m_quadVbo.create();
    m_quadVbo.bind();
    m_quadVbo.allocate(quadVertices, sizeof(quadVertices));

    // [Layout 0] Quad Vertex Position (vec2)
    // 이 속성은 인스턴스마다 변하지 않고, 사각형 그릴 때마다 재사용됨
    m_program->enableAttributeArray(0);
    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 2, 2 * sizeof(float));

    // 2. 인스턴스 데이터용 VBO 생성 (아직 데이터는 없음)
    m_instanceVbo.create();
    m_instanceVbo.bind();

    m_vao.release();
    m_instanceVbo.release(); // quadVbo는 release 안 해도 됨 (다음 바인딩 때 풀림)
#endif

    // 3. FBO 생성 (1280x720 고정 해상도, Depth/Stencil 포함)
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    m_fbo = new QOpenGLFramebufferObject(INTERNAL_WIDTH, INTERNAL_HEIGHT, format);

    if (m_fbo->isValid()) {
        qDebug() << "FBO Created Successfully: 1280x720";
    } else {
        qCritical() << "FBO Creation Failed!";
    }
}

void SplattingWidget::resizeGL(int w, int h)
{
    // 윈도우 크기가 변해도 FBO 크기는 고정(1280x720)이므로
    // 여기서 FBO를 재생성하지 않습니다.
    // 대신 화면 갱신 요청만 합니다.
    update();
}

// [추가] 마우스 이벤트 구현
void SplattingWidget::mousePressEvent(QMouseEvent *event)
{
    m_camera.handleMousePress(event);
    m_needsSort = true;
    update(); // 화면 갱신 요청
}

void SplattingWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_camera.handleMouseMove(event);
    m_needsSort = true;
    update(); // 화면 갱신 요청
}

void SplattingWidget::wheelEvent(QWheelEvent *event)
{
    m_camera.handleWheel(event);
    m_needsSort = true;
    update(); // 화면 갱신 요청
}

void SplattingWidget::paintGL()
{
    // 1. FPS 계산
    m_frameCount++;
    if (m_fpsTimer.elapsed() >= 1000) {
        m_currentFps = m_frameCount / (m_fpsTimer.elapsed() / 1000.0f);
        m_frameCount = 0;
        m_fpsTimer.restart();
    }

    if (!m_fbo || !m_fbo->isValid()) return;

    // 1. 카메라 행렬 가져오기
    QMatrix4x4 view = m_camera.getViewMatrix();

    // 2. [최적화] 정렬은 "필요할 때(마우스 움직임)"만 수행
    if (m_needsSort) {
        sortSplats(view);

        // 정렬된 데이터 재전송
        if (m_splatCount > 0) {
            m_instanceVbo.bind();
            m_instanceVbo.write(0, m_splats.data(), m_splatCount * sizeof(RenderSplat));
            m_instanceVbo.release();
        }
        m_needsSort = false; // 정렬 완료
    }

    // 정렬된 데이터를 GPU로 재전송 (VBO Update)
    if (m_splatCount > 0) {
        m_instanceVbo.bind();
        // 메모리 전체를 덮어쓰기 (write는 allocate보다 빠름)
        m_instanceVbo.write(0, m_splats.data(), m_splatCount * sizeof(RenderSplat));
        m_instanceVbo.release();
    }

    // --- [Step 1: Off-screen Rendering] ---
    m_fbo->bind(); // FBO에 그리기 시작
    
    // 뷰포트를 FBO 크기(720p)로 설정
    glViewport(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT);
    
    // 배경 지우기 (어두운 회색)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 블렌딩 설정
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 일반적인 투명도 합성

    // 깊이 쓰기 비활성화 (Depth Mask False)
    // 투명한 물체끼리 겹칠 때 뒤쪽 물체가 안 그려지는 문제 방지
    // (정석은 정렬(Sorting)을 해야 하지만, 일단 이 방법으로도 꽤 그럴싸하게 보입니다)
    glDepthMask(GL_FALSE);

    // 빨간 삼각형 그리기
    if (m_program->bind()) {
        // [핵심] 카메라 행렬 계산 (Projection * View)
        QMatrix4x4 view = m_camera.getViewMatrix();
        QMatrix4x4 proj = m_camera.getProjectionMatrix((float)INTERNAL_WIDTH / INTERNAL_HEIGHT);
        QMatrix4x4 vp = proj * view; // View-Projection Matrix

        m_program->setUniformValue("vp_matrix", vp); // 이름 변경 mvp -> vp

        // 카메라의 Right, Up 벡터를 추출하여 셰이더로 보냄 (빌보딩용)
        // View Matrix의 1열(Right), 2열(Up)을 가져옴 (Qt는 Column-Major)
        QVector3D cameraRight(view(0, 0), view(1, 0), view(2, 0));
        QVector3D cameraUp(view(0, 1), view(1, 1), view(2, 1));

        m_program->setUniformValue("cameraRight", cameraRight);
        m_program->setUniformValue("cameraUp", cameraUp);

        // UI 제어 변수 전달 (셰이더에 uniform 추가 필요!)
        m_program->setUniformValue("uGlobalScale", m_globalScale);
        m_program->setUniformValue("uAlphaCutoff", m_alphaCutoff);

        m_vao.bind();
#if 0
        glDrawArrays(GL_TRIANGLES, 0, 3);
#else
        // [수정] 점 그리기
        // 데이터가 없으면(0개) 그리지 않음
        if (m_splatCount > 0) {
            // 인스턴싱 드로우 콜
            // 사각형(정점 4개)을 m_splatCount 만큼 반복해서 그림
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, m_splatCount);
        }
#endif
        m_vao.release();
        m_program->release();
    }

    // 상태 복구 (다음 프레임을 위해)
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    m_fbo->release(); // FBO 그리기 종료 (다시 기본 프레임버퍼로 돌아옴)


    // --- [Step 2: Upscaling to Screen] ---
    // 현재 윈도우 크기(this->width(), this->height())에 맞춰 FBO 내용을 복사(Blit)합니다.
    // OpenGL의 Blit 기능을 사용하면 하드웨어 가속을 통해 자동으로 스케일링됩니다.
    
    // 읽기 버퍼: 우리가 그린 FBO
    // 쓰기 버퍼: 현재 화면 (ID 0 또는 Qt가 관리하는 기본 FBO)
    QOpenGLFramebufferObject::blitFramebuffer(
        nullptr,            // 타겟 (nullptr이면 현재 바인딩된 기본 FBO = 화면)
        QRect(0, 0, width(), height()),     // 타겟 영역 (창 전체)
        m_fbo,              // 소스 FBO
        QRect(0, 0, INTERNAL_WIDTH, INTERNAL_HEIGHT), // 소스 영역 (720p)
        GL_COLOR_BUFFER_BIT, 
        m_useLinearFilter ? GL_LINEAR : GL_NEAREST           // 필터링: GL_LINEAR(부드럽게), GL_NEAREST(픽셀화)
    );

    // 5. QPainter로 FPS 텍스트 오버레이
    // OpenGL 렌더링 후 QPainter를 쓰면 위에 덧그려짐
    QPainter painter(this);
    painter.setPen(Qt::yellow);
    painter.setFont(QFont("Arial", 14, QFont::Bold));
    painter.drawText(20, 30, QString("FPS: %1").arg(QString::number(m_currentFps, 'f', 1)));
    painter.drawText(20, 50, QString("Points: %1").arg(m_splatCount));
    painter.end();
}

void SplattingWidget::initShaders()
{
    m_program = new QOpenGLShaderProgram();

    // Vertex Shader
    const char *vshader = R"(
        #version 330 core
        layout(location = 0) in vec2 aQuadPos;
        layout(location = 1) in vec3 aInstPos;
        layout(location = 2) in vec3 aInstColor;
        layout(location = 3) in float aInstOpacity;
        layout(location = 4) in vec3 aInstScale;

        uniform mat4 vp_matrix;
        uniform vec3 cameraRight;
        uniform vec3 cameraUp;
        uniform float uGlobalScale;

        out vec3 vColor;
        out vec2 vQuadPos;
        out float vOpacity;

        void main() {
            // UI에서 받은 스케일 적용
            float scaleFactor = uGlobalScale;

            vec3 worldPos = aInstPos
                          + (cameraRight * aQuadPos.x * aInstScale.x * scaleFactor)
                          + (cameraUp * aQuadPos.y * aInstScale.y * scaleFactor);

            gl_Position = vp_matrix * vec4(worldPos, 1.0);
            vColor = aInstColor;
            vQuadPos = aQuadPos;
            vOpacity = aInstOpacity;
        }
    )";

    // Fragment Shader (가우시안 효과의 핵심)
    const char *fshader = R"(
        #version 330 core
        in vec3 vColor;
        in vec2 vQuadPos;
        in float vOpacity;

        uniform float uAlphaCutoff;

        out vec4 FragColor;

        void main() {
            // 중심에서의 거리 제곱 (x^2 + y^2)
            float distSq = dot(vQuadPos, vQuadPos);

            // 1. 원형 클리핑: 반지름 1을 넘으면 그리지 않고 버림 (사각형을 원으로 만듦)
            if (distSq > 1.0) discard;

            // 2. 가우시안 감쇠 (Gaussian Falloff)
            // 중심(0)일 때 1, 가장자리(1)로 갈수록 0에 가깝게 줄어듦
            // exp(-x) 그래프 형태를 사용
            float alpha = vOpacity * exp(-distSq * 3.0);

            // UI에서 받은 컷오프 적용
            if (alpha < uAlphaCutoff) discard;

            FragColor = vec4(vColor, alpha);
        }
    )";

    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vshader);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fshader);
    m_program->link();
}

#if 0
void SplattingWidget::initGeometry()
{
    // 화면 중앙에 위치할 삼각형 정점 데이터
    float vertices[] = {
        -0.5f, -0.5f, 0.0f, // 좌측 하단
         0.5f, -0.5f, 0.0f, // 우측 하단
         0.0f,  0.5f, 0.0f  // 상단 중앙
    };

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));

    // Shader의 layout(location=0)에 연결
    m_program->enableAttributeArray(0);
    m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * sizeof(float));

    m_vao.release();
    m_vbo.release();
}
#endif

void SplattingWidget::sortSplats(const QMatrix4x4& viewMatrix)
{
    if (m_splats.empty()) return;

    // View Matrix의 3행(Row 2)이 카메라의 Z축(Look Vector) 방향입니다.
    // (Qt QMatrix4x4는 row(행), column(열) 접근 가능)
    // 3D 공간의 점 P와 카메라 Z축 벡터의 내적(Dot Product)을 구하면 깊이가 나옵니다.

    // 최적화를 위해 View Matrix의 Z축 벡터 요소를 미리 뺍니다.
    // 주의: OpenGL에서는 카메라가 -Z를 바라보지만, 거리 계산엔 Z축 계수를 씁니다.
    float viewZ_x = viewMatrix(0, 2);
    float viewZ_y = viewMatrix(1, 2);
    float viewZ_z = viewMatrix(2, 2);
    float viewZ_w = viewMatrix(3, 2); // Translation 관련

    // std::sort 사용 (Lambda 함수)
    // "누가 더 뒤에 있니?" (Depth가 큰 순서대로 내림차순 정렬)
    std::sort(m_splats.begin(), m_splats.end(),
              [=](const RenderSplat& a, const RenderSplat& b) {
                  // 깊이(Depth) = ViewMatrix * Position 의 Z값
                  // 행렬 곱셈 공식을 풀어서 씀 (성능 위해)
                  float depthA = (viewZ_x * a.x) + (viewZ_y * a.y) + (viewZ_z * a.z) + viewZ_w;
                  float depthB = (viewZ_x * b.x) + (viewZ_y * b.y) + (viewZ_z * b.z) + viewZ_w;

                  // 멀리 있는 것(Depth가 큰 것)이 먼저 그려져야 함 -> 내림차순
                  return depthA > depthB;
              }
              );
}
