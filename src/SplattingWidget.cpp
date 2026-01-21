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

void SplattingWidget::setShapness(float value) {
    m_sharpness = value;
    update();
}

void SplattingWidget::setUpscaleFilter(bool isLinear) {
    m_useLinearFilter = isLinear;
    update(); // 즉시 다시 그리기
}

void SplattingWidget::setUseFSR(bool use) {
    m_useFSR = use;
    update();
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

    initFSRQuad();

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


    if(m_useFSR)
    {
        // 2. On-screen Rendering (화면 늘리기 + FSR 적용)
        QSize windowSize = this->size(); // 현재 윈도우 크기 (예: 4K)
        glViewport(0, 0, windowSize.width(), windowSize.height());
        glClear(GL_COLOR_BUFFER_BIT);

        m_fsrShader->bind();

        // 보통 Post-processing 단계에서는 덮어쓰기(Replace)가 정석이므로
        // 블렌딩을 끄는 것이 깔끔할 수 있습니다.
        glDisable(GL_BLEND);

        // 만약 배경 투명도를 살려야 한다면 아래 코드 사용:
        // glEnable(GL_BLEND);
        // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // FBO 텍스처를 0번 슬롯에 바인딩
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_fbo->texture());

        // Uniform 값 전달
        m_fsrShader->setUniformValue("screenTexture", 0);
        m_fsrShader->setUniformValue("sharpness", m_sharpness); // 0.0 ~ 1.0 값 조절

        renderFSRQuad();

        m_fsrShader->release();
    }
    else
    {
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
    }

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

    const char *fsrvshader = R"(
        #version 450 core

        layout(location = 0) in vec2 aPos;      // Attribute 0: 위치
        layout(location = 1) in vec2 aTexCoord; // Attribute 1: UV

        out vec2 vTexCoord; // Fragment Shader(fsr.frag)로 넘겨줄 UV

        void main()
        {
            vTexCoord = aTexCoord;
            // 입력된 XY 좌표(-1 ~ 1)를 그대로 사용하여 화면 전체를 덮음
            gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
        }
    )";

    const char *fsrfshader = R"(
        #version 450 core

        in vec2 vTexCoord;
        out vec4 outColor;

        uniform sampler2D screenTexture;
        uniform float sharpness; // 0.0 ~ 1.0

        // RGB를 루마(밝기)로 변환하는 함수
        float GetLuma(vec3 rgb) {
            return dot(rgb, vec3(0.299, 0.587, 0.114));
        }

        vec4 FsrRcas(vec2 uv) {
            ivec2 texSize = textureSize(screenTexture, 0);
            ivec2 coord = ivec2(uv * vec2(texSize));

            // [1] 샘플링 (RGB + Alpha)
            vec4 c_full = texelFetch(screenTexture, coord, 0);
            vec4 t_full = texelFetch(screenTexture, coord + ivec2(0, -1), 0);
            vec4 b_full = texelFetch(screenTexture, coord + ivec2(0, 1), 0);
            vec4 l_full = texelFetch(screenTexture, coord + ivec2(-1, 0), 0);
            vec4 r_full = texelFetch(screenTexture, coord + ivec2(1, 0), 0);

            vec3 c = c_full.rgb;
            vec3 t = t_full.rgb;
            vec3 b = b_full.rgb;
            vec3 l = l_full.rgb;
            vec3 r = r_full.rgb;
            float alpha = c_full.a;

            // [2] 컨트라스트 분석 (밴딩 방지 핵심)
            // 주변 픽셀들의 밝기 차이가 거의 없다면(평평한 면), 샤픈을 주면 안 됩니다.
            // 여기서 억지로 샤픈을 주면 '이상한 선'이 생깁니다.
            float lumaC = GetLuma(c);
            float lumaT = GetLuma(t);
            float lumaB = GetLuma(b);
            float lumaL = GetLuma(l);
            float lumaR = GetLuma(r);

            // 주변 밝기의 최대/최소 차이 계산
            float minLuma = min(lumaC, min(min(lumaT, lumaB), min(lumaL, lumaR)));
            float maxLuma = max(lumaC, max(max(lumaT, lumaB), max(lumaL, lumaR)));
            float range = maxLuma - minLuma;

            // [3] 샤픈 강도 동적 조절 (Adaptive Sharpening)
            // range(밝기 차이)가 너무 작으면 샤프니스 강도(w)를 0으로 만듭니다.
            // 즉, 노이즈나 그라데이션에서는 작동을 멈춥니다.

            float sharpLinear = clamp(sharpness, 0.0, 1.0);

            // 기본 가중치 (-0.5 ~ 0.0) -> 값을 조금 더 부드럽게 낮췄습니다.
            float w = sharpLinear * -0.15;

            // 범위가 0에 가까우면 가중치도 0으로 (Zero division 방지 및 노이즈 제거)
            if (range < 0.001) {
                w = 0.0;
            }

            // [4] 컨볼루션 연산
            vec3 neighbors = t + b + l + r;
            vec3 result_rgb = (c + w * neighbors) / (1.0 + 4.0 * w);

            // [5] 최종 Clamping (0~1 범위만 제한)
            // 아까 문제가 됐던 bMin/bMax 클램핑은 삭제했습니다.
            // 대신 기본적인 0.0~1.0 오버플로우만 막습니다.
            result_rgb = clamp(result_rgb, 0.0, 1.0);

            return vec4(result_rgb, alpha);
        }

        void main() {
            outColor = FsrRcas(vTexCoord);
        }
    )";

    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vshader);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fshader);
    m_program->link();

    m_fsrShader = new QOpenGLShaderProgram;
    m_fsrShader->addShaderFromSourceCode(QOpenGLShader::Vertex, fsrvshader);
    m_fsrShader->addShaderFromSourceCode(QOpenGLShader::Fragment, fsrfshader);
    m_fsrShader->link();
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

void SplattingWidget::initFSRQuad()
{
    // 화면 전체를 덮는 사각형 정점 데이터 (Triangle Strip 사용)
    // 포맷: { X, Y,   U, V }
    float quadVertices[] = {
        // 위치(XY)    // 텍스처 좌표(UV)
        -1.0f,  1.0f,  0.0f, 1.0f, // 왼쪽 위
        -1.0f, -1.0f,  0.0f, 0.0f, // 왼쪽 아래
        1.0f,  1.0f,  1.0f, 1.0f, // 오른쪽 위
        1.0f, -1.0f,  1.0f, 0.0f  // 오른쪽 아래
    };

    m_fsrvao.create();
    m_fsrvao.bind();

    m_fsrquadVBO.create();
    m_fsrquadVBO.bind();
    m_fsrquadVBO.allocate(quadVertices, sizeof(quadVertices));

    // Attribute 0: 위치 (vec2 position)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // Attribute 1: 텍스처 좌표 (vec2 texCoord)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    m_fsrquadVBO.release();
    m_fsrvao.release();
}

void SplattingWidget::renderFSRQuad()
{
    m_fsrvao.bind();
    // Triangle Strip 모드로 4개의 점을 이어 사각형을 그립니다.
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_fsrvao.release();
}

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
