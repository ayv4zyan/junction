cmake_minimum_required(VERSION 3.21)

project(Junction VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Svg Network Quick QuickControls2 Test QuickTest)
qt_standard_project_setup(REQUIRES 6.9)

# Every QML module in one folder, so they find each other (the app imports JunctionDesign).
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

add_subdirectory(src/core)
add_subdirectory(src/render)
add_subdirectory(src/design)
add_subdirectory(src/app)

enable_testing()
add_subdirectory(tests)
