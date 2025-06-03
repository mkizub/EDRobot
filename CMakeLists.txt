cmake_minimum_required(VERSION 3.26)

set (CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo)

project(EDRobot)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS TRUE) # FALSE

set(OpenCV_ROOT "${VCPKG_INSTALLED_DIR}/x64-windows/share/opencv4")
find_package(OpenCV REQUIRED)

#if (BUILD_SHARED_LIBS)
#    set(OpenCV_STATIC FALSE)
#    set(OpenCV_SHARED TRUE)
#    if (MSVC)
#        set(OpenCV_DIR "../opencv-4.x/vs-dist-${CMAKE_BUILD_TYPE}")
#    else()
#        set(OpenCV_DIR "../opencv-4.x/dist-${CMAKE_BUILD_TYPE}-dll")
#    endif()
#else()
#    set(OpenCV_STATIC TRUE)
#    set(OpenCV_SHARED FALSE)
#    if (MSVC)
#        set(OpenCV_DIR "../opencv-4.x/vs-dist")
#    else()
#        set(OpenCV_DIR "../opencv-4.x/dist-${CMAKE_BUILD_TYPE}")
#    endif()
#endif()
#find_package(OpenCV REQUIRED)

#add_compile_definitions(UNICODE)
add_compile_definitions(NOMINMAX)
add_compile_definitions(ELPP_NO_DEFAULT_LOG_FILE)
if (MSVC)
    add_compile_options(-Zc:__cplusplus)
endif()

#include(FetchContent)
#FetchContent_Declare(
#        cpptrace
#        GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
#        GIT_TAG        v0.8.3 # <HASH or TAG>
#)
#FetchContent_MakeAvailable(cpptrace)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_package(cpptrace CONFIG REQUIRED)
endif()

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
#target_precompile_headers(EDRobot PUBLIC pch.h)
target_include_directories(EDRobot PUBLIC ${OpenCV_INCLUDE_DIRS})
target_include_directories(EDRobot PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(EDRobot ${OpenCV_LIBS} winmm) # cpptrace::cpptrace msimg32
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(EDRobot cpptrace::cpptrace)
endif()

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
#add_custom_command(TARGET EDRobot POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:EDRobot> $<TARGET_RUNTIME_DLLS:EDRobot>
#        COMMAND_EXPAND_LISTS
#)

install(TARGETS EDRobot RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-win-.*\.dll" "ext-ms-.*\.dll"
        POST_EXCLUDE_REGEXES [[.*system32.*\.dll]]
)
#install(IMPORTED_RUNTIME_ARTIFACTS cpptrace::cpptrace)
#install(IMPORTED_RUNTIME_ARTIFACTS opencv_core opencv_imgproc opencv_imgcodecs opencv_highgui opencv_videoio)
install(DIRECTORY templates DESTINATION "." FILES_MATCHING PATTERN "*.png")
file(GLOB EDROBOT_CONFIGS *.json5)
install(FILES ${EDROBOT_CONFIGS} logging.conf DESTINATION ".")
install(FILES EDRobot.cmd.in DESTINATION "." RENAME EDRobot.cmd)

