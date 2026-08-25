cmake_minimum_required(VERSION 4.0)
project(Qt-WindowsApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Network)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
find_package(Qt6 REQUIRED COMPONENTS Charts)
find_package(OpenCV REQUIRED)

qt_standard_project_setup()


add_executable(Qt-WindowsApp main.cpp
        controllers/MainController.h
        controllers/MainController.cpp
        frontend/MainWindow.cpp
        frontend/MainWindow.h
        frontend/MainWindow.ui
        controllers/MainController.cpp
        controllers/MainController.h
        backend/models/AppState.cpp
        backend/models/AppState.h
        backend/services/AppService.cpp
        backend/services/AppService.h
        resources/resources.qrc
        resources/styles.qss
        frontend/Animator.cpp
        frontend/Animator.h
        backend/Network/UdpClient.cpp
        backend/Network/UdpClient.h
        frontend/notification/NotificationPop.cpp
        frontend/notification/NotificationPop.h
        frontend/graph/graphData.cpp
        frontend/graph/graphData.h
        frontend/testSliders/SliderPanel.cpp
        frontend/testSliders/SliderPanel.h
        frontend/video/VideoView.cpp
        frontend/video/VideoView.h
        frontend/camera/CameraPanel.cpp
        frontend/camera/CameraPanel.h
        backend/vision/VisionWorker.cpp
        backend/vision/VisionWorker.h
        backend/vision/CameraEnumerator.cpp
        backend/vision/CameraEnumerator.h
        backend/geometry/Triangulator.cpp
        backend/geometry/Triangulator.h
)
add_executable(triangulator_test backend/geometry/Triangulator.cpp
backend/geometry/Triangulator.h
        backend/geometry/TriangulatorTest.cpp)
target_include_directories(Qt-WindowsApp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/frontend)

target_link_libraries(Qt-WindowsApp PRIVATE Qt6::Widgets)
target_link_libraries(Qt-WindowsApp PRIVATE Qt6::Network)
target_link_libraries(Qt-WindowsApp PRIVATE Qt6::OpenGLWidgets)
target_link_libraries(Qt-WindowsApp PRIVATE Qt6::Charts)
target_link_libraries(Qt-WindowsApp PRIVATE ole32 oleaut32 strmiids)
target_link_libraries(Qt-WindowsApp PRIVATE ${OpenCV_LIBS})
