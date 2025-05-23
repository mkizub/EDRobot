cmake_minimum_required(VERSION 3.26)
project(EDRobot)

set(CMAKE_CXX_STANDARD 23)

#set(BUILD_SHARED_LIBS OFF)
#set(BUILD_TESTS OFF)
#set(BUILD_EXAMPLES OFF)
#set(BUILD_LIST core imgproc imgcodecs highgui features2d objdetect dnn ml stitching)
#add_subdirectory("opencv")

set(OpenCV_DIR "../opencv-4.x/dist")
set(OpenCV_STATIC TRUE)
set(OpenCV_SHARED FALSE)
find_package(OpenCV REQUIRED)

#set(FLTK_DIR "fltk-1.4.3/dist/CMake")
#set(FLTK_SKIP_OPENGL TRUE)
#set(FLTK_SKIP_FLUID TRUE)
#find_package(FLTK REQUIRED)
set(FLTK_INCLUDE_DIR "../fltk-1.4.3/dist/include")
set(FLTK_LIB_PATH    "../fltk-1.4.3/dist/lib")
set(FLTK_LIBRARIES   libfltk.a comctl32 ws2_32 gdiplus )


add_executable(EDRobot main.cpp
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
)

target_include_directories(EDRobot PUBLIC ${OpenCV_INCLUDE_DIRS} ${FLTK_INCLUDE_DIR})
target_link_directories(EDRobot PRIVATE ${FLTK_LIB_PATH})
target_link_libraries(EDRobot ${OpenCV_LIBS} ${FLTK_LIBRARIES} )
