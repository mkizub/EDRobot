cmake_minimum_required(VERSION 3.26)

set (CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo)

project(EDRobot)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS FALSE)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set(OpenCV_DIR "../opencv-4.x/vs-dist")
else()
    set(OpenCV_DIR "../opencv-4.x/dist-${CMAKE_BUILD_TYPE}")
endif()
set(OpenCV_STATIC TRUE)
set(OpenCV_SHARED FALSE)
find_package(OpenCV REQUIRED)

#add_compile_definitions(UNICODE)
add_compile_definitions(NOMINMAX)
add_compile_definitions(ELPP_NO_DEFAULT_LOG_FILE)

include(FetchContent)
FetchContent_Declare(
        cpptrace
        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
        GIT_TAG        v0.8.3 # <HASH or TAG>
)
FetchContent_MakeAvailable(cpptrace)

add_executable(EDRobot
        pch.cpp
        pch.h
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
        EDWidget.cpp
        EDWidget.h
)
# target_precompile_headers(EDRobot PUBLIC pch.h)
target_include_directories(EDRobot PUBLIC ${OpenCV_INCLUDE_DIRS})
target_include_directories(EDRobot PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(EDRobot ${OpenCV_LIBS} cpptrace::cpptrace winmm) # cpptrace::cpptrace msimg32

set_target_properties(EDRobot PROPERTIES DEBUG_POSTFIX "Debug")

# Needed for shared library builds on windows:  copy cpptrace.dll to the same directory as the
# executable for your_target
#if(WIN32 AND BUILD_SHARED_LIBS)
#    add_custom_command(
#            TARGET EDRobot POST_BUILD
#            COMMAND ${CMAKE_COMMAND} -E copy_if_different
#            $<TARGET_FILE:cpptrace::cpptrace>
#            $<TARGET_FILE_DIR:EDRobot>
#    )
#endif()
