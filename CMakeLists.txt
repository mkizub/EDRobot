cmake_minimum_required(VERSION 3.26)
project(EDRobot)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS FALSE)

set(OpenCV_DIR "../opencv-4.x/dist-${CMAKE_BUILD_TYPE}")
set(OpenCV_STATIC TRUE)
set(OpenCV_SHARED FALSE)
find_package(OpenCV REQUIRED)

include(FetchContent)
FetchContent_Declare(
        cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG        v0.8.3 # <HASH or TAG>
)
FetchContent_MakeAvailable(cpptrace)

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
target_link_libraries(EDRobot ${OpenCV_LIBS} cpptrace::cpptrace)

set_target_properties(EDRobot PROPERTIES DEBUG_POSTFIX "Debug")

# Needed for shared library builds on windows:  copy cpptrace.dll to the same directory as the
# executable for your_target
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(
            TARGET EDRobot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:cpptrace::cpptrace>
            $<TARGET_FILE_DIR:EDRobot>
    )
endif()
