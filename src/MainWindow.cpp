#include "MainWindow.h"
#include "SplattingWidget.h"
#include "PlyLoader.h"
#include <QFile>
#include <QDataStream>
#include <QtMath>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QGroupBox>
#include <QCheckBox>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 초기 해상도를 FHD(1920x1080)으로 변경
    // 내부 렌더링은 720p이므로, 실행하자마자 확대된 화면을 보게 됨
    resize(1920, 1080);
    setWindowTitle("Switch 2 Style Viewer - 720p to 1080p Upscaling Test");

    // 1. 메인 위젯 설정
    m_splatWidget = new SplattingWidget(this);
    setCentralWidget(m_splatWidget);

    // 2. 메뉴바 설정
    QMenu *fileMenu = menuBar()->addMenu("File");
    QAction *openAction = fileMenu->addAction("Open .ply");
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenActionTriggered);

    // 3. [핵심] 제어 패널 (Control Panel) 추가
    QDockWidget *dock = new QDockWidget("Rendering Controls", this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget *panel = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(panel);

    // (1) Scale Slider
    QGroupBox *scaleGroup = new QGroupBox("Splat Scale");
    QVBoxLayout *scaleLayout = new QVBoxLayout(scaleGroup);
    QSlider *scaleSlider = new QSlider(Qt::Horizontal);
    scaleSlider->setRange(1, 200); // 0.01 ~ 2.00
    scaleSlider->setValue(100);    // Default 1.0
    scaleLayout->addWidget(scaleSlider);
    layout->addWidget(scaleGroup);

    // (2) Alpha Cutoff Slider
    QGroupBox *alphaGroup = new QGroupBox("Alpha Cutoff (Clean-up)");
    QVBoxLayout *alphaLayout = new QVBoxLayout(alphaGroup);
    QSlider *alphaSlider = new QSlider(Qt::Horizontal);
    alphaSlider->setRange(0, 100); // 0.00 ~ 1.00
    alphaSlider->setValue(5);      // Default 0.05
    alphaLayout->addWidget(alphaSlider);
    layout->addWidget(alphaGroup);

    // (3) Upscaling Filter Checkbox (컨트롤 패널에 추가)
    QGroupBox *filterGroup = new QGroupBox("Upscaling Filter");
    QVBoxLayout *filterLayout = new QVBoxLayout(filterGroup);
    QCheckBox *filterCheck = new QCheckBox("Smooth (Linear Filtering)");
    filterCheck->setChecked(true); // 기본은 부드럽게
    filterLayout->addWidget(filterCheck);
    layout->addWidget(filterGroup); // 레이아웃에 추가

    layout->addStretch(); // 나머지 공간 채우기
    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    // 4. 슬라이더 이벤트 연결
    connect(scaleSlider, &QSlider::valueChanged, [this](int value){
        float scale = value / 100.0f;
        m_splatWidget->setGlobalScale(scale);
    });

    connect(alphaSlider, &QSlider::valueChanged, [this](int value){
        float cutoff = value / 100.0f;
        m_splatWidget->setAlphaCutoff(cutoff);
    });

    connect(filterCheck, &QCheckBox::toggled, [this](bool checked){
        // 체크되면 Linear(부드럽게), 해제되면 Nearest(각지게)
        m_splatWidget->setUpscaleFilter(checked);
    });

    // 샘플 ply 파일 만들기 위한 코드
    //createDummyPly("d:/test_cube.ply");
}

MainWindow::~MainWindow()
{
}

void MainWindow::createDummyPly(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) return;

    int count = 1000; // 점 1000개 생성

    // 1. 헤더 작성
    QTextStream out(&file);
    out << "ply\n";
    out << "format binary_little_endian 1.0\n";
    out << "element vertex " << count << "\n";
    out << "property float x\nproperty float y\nproperty float z\n";
    out << "property float nx\nproperty float ny\nproperty float nz\n";
    out << "property float f_dc_0\nproperty float f_dc_1\nproperty float f_dc_2\n";
    // f_rest 45개
    for(int i=0; i<45; ++i) out << "property float f_rest_" << i << "\n";
    out << "property float opacity\n";
    out << "property float scale_0\nproperty float scale_1\nproperty float scale_2\n";
    out << "property float rot_0\nproperty float rot_1\nproperty float rot_2\nproperty float rot_3\n";
    out << "end_header\n";
    out.flush();

    // 2. 바이너리 데이터 작성
    // (x,y,z, nx,ny,nz, f_dc(3), f_rest(45), op, scale(3), rot(4)) = 총 62 floats
    for (int i = 0; i < count; ++i) {
        float raw[62] = {0};

        // 위치: 랜덤하게 -1 ~ 1 사이
        raw[0] = ((rand() % 100) / 50.0f) - 1.0f; // x
        raw[1] = ((rand() % 100) / 50.0f) - 1.0f; // y
        raw[2] = ((rand() % 100) / 50.0f) - 1.0f; // z

        // 색상 (f_dc): 랜덤
        raw[6] = (rand() % 100) / 100.0f;
        raw[7] = (rand() % 100) / 100.0f;
        raw[8] = (rand() % 100) / 100.0f;

        // 투명도 (Sigmoid 역함수 흉내, 대충 큰 값 넣으면 불투명해짐)
        raw[54] = 5.0f;

        // 스케일 (Log 스케일이므로 -2 정도 넣으면 작게 나옴)
        raw[55] = -2.0f; raw[56] = -2.0f; raw[57] = -2.0f;

        // 회전 (Identity Quaternion)
        raw[58] = 1.0f;

        file.write(reinterpret_cast<const char*>(raw), sizeof(raw));
    }
    file.close();
}

void MainWindow::onOpenActionTriggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open Gaussian Splatting PLY", "", "PLY Files (*.ply)");

    if (!fileName.isEmpty()) {
        PlyLoader loader;
        std::vector<RenderSplat> splats;

        // 로딩 시작 로그
        qDebug() << "Start loading PLY...";

        if (loader.loadPly(fileName, splats)) {
            qDebug() << "Loaded" << splats.size() << "points. Uploading to GPU...";

            // [연결] 위젯에 데이터 전달
            m_splatWidget->loadData(splats);

            qDebug() << "Upload Complete!";
        } else {
            qCritical() << "Failed to load PLY.";
        }
    }
}
