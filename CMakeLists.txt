cmake_minimum_required(VERSION 3.26)
project(EDRobot)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS FALSE)

set(OpenCV_DIR "../opencv-4.x/dist-${CMAKE_BUILD_TYPE}")
set(OpenCV_STATIC TRUE)
set(OpenCV_SHARED FALSE)
find_package(OpenCV REQUIRED)

add_executable(EDRobot
        main.cpp
        Keyboard.cpp
        Keyboard.h
        Master.cpp
        Master.h
        Task.cpp
        Task.h
        Template.cpp
        Template.h
        easylogging++.cc
        easylogging++.h
        Utils.cpp
        Utils.h
        Capturer.cpp
        Capturer.h
        UI.cpp
        UI.h
        ui/Dialogs.rc
)
target_include_directories(EDRobot PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(EDRobot ${OpenCV_LIBS} ${MFC_LIBRARIES} )
