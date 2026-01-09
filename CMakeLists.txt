cmake_minimum_required(VERSION 3.16)

project(Switch2SplatViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)   # Q_OBJECT 매크로 자동 처리
set(CMAKE_AUTORCC ON)   # 리소스 파일(.qrc) 자동 처리
set(CMAKE_AUTOUIC ON)   # UI 파일(.ui) 자동 처리

# Qt 6 필수 컴포넌트 찾기
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets)

# 소스 파일 지정
set(PROJECT_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/SplattingWidget.cpp
    src/SplattingWidget.h
    src/Camera.cpp
    src/Camera.h
    src/GaussianData.h
    src/PlyLoader.cpp
    src/PlyLoader.h
)

add_executable(Switch2SplatViewer ${PROJECT_SOURCES})

# 라이브러리 링크
target_link_libraries(Switch2SplatViewer PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGLWidgets)

# 윈도우 앱 설정 (콘솔창 숨김 해제 - 디버깅용으로 당분간 콘솔 켜둠)
# set_target_properties(Switch2SplatViewer PROPERTIES WIN32_EXECUTABLE ON)
