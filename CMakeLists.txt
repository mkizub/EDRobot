cmake_minimum_required(VERSION 3.26)

set (CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo)

project(EDRobot)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS TRUE)

set(OpenCV_ROOT "${VCPKG_INSTALLED_DIR}/x64-windows/share/opencv4")
find_package(OpenCV REQUIRED)

find_package(Tesseract CONFIG REQUIRED)

#find_package(Gettext REQUIRED)
find_library(GETTEXT_LIBRARIES intl iconv)


add_compile_definitions(UNICODE)
add_compile_definitions(NOMINMAX)
add_compile_definitions(ELPP_NO_DEFAULT_LOG_FILE ELPP_THREAD_SAFE)
if (MSVC)
    add_compile_options(-Zc:__cplusplus)
endif()

find_package(cpptrace CONFIG REQUIRED)

add_executable(EDRobot
        src/pch.cpp
        src/pch.h
        src/main.cpp
        src/Capturer.cpp
        src/Capturer.h
        src/Configuration.cpp
        src/Configuration.h
        src/EDWidget.cpp
        src/EDWidget.h
        src/Keyboard.cpp
        src/Keyboard.h
        src/Master.cpp
        src/Master.h
        src/Task.cpp
        src/Task.h
        src/Template.cpp
        src/Template.h
        src/UI.h
        src/Utils.cpp
        src/Utils.h
        ui/Dialogs.rc
        thirdparty/easylogging/easylogging++.cc
        thirdparty/easylogging/easylogging++.h
        thirdparty/dirlistener/ReadDirectoryChanges.cpp
        thirdparty/dirlistener/ReadDirectoryChanges.h
        thirdparty/dirlistener/ReadDirectoryChangesPrivate.cpp
        thirdparty/dirlistener/ReadDirectoryChangesPrivate.h
        thirdparty/dirlistener/ThreadSafeQueue.h
        thirdparty/CLI11/CLI11.hpp
        thirdparty/xml/xml.h
        src/FuzzyMatch.cpp
        src/FuzzyMatch.h
        src/ui/UIToast.cpp
        src/ui/UIToast.h
        src/ui/UIManager.cpp
        src/ui/UIManager.h
        src/ui/UIStartupDialog.cpp
        src/ui/UIStartupDialog.h
        src/ui/UISellInput.cpp
        src/ui/UISellInput.h
        src/ui/UICalibration.cpp
        src/ui/UICalibration.h
        src/ui/UISelectRect.cpp
        src/ui/UISelectRect.h
        src/ui/UIDialog.cpp
        src/ui/UIDialog.h
        src/ui/UIWindow.cpp
        src/ui/UIWindow.h
)
#target_precompile_headers(EDRobot PUBLIC pch.h)
target_include_directories(EDRobot PRIVATE ${OpenCV_INCLUDE_DIRS})
target_include_directories(EDRobot PRIVATE "${VCPKG_INSTALLED_DIR}/x64-windows/include")
target_include_directories(EDRobot PRIVATE thirdparty)
#target_include_directories(EDRobot PRIVATE ${GETTEXT_INCLUDE_DIR})
target_link_libraries(EDRobot ${GETTEXT_LIBRARIES})
target_link_libraries(EDRobot ${OpenCV_LIBS} Tesseract::libtesseract winmm shcore) # cpptrace::cpptrace msimg32
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(EDRobot cpptrace::cpptrace)
endif()

install(TARGETS EDRobot RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-win-.*\.dll" "ext-ms-.*\.dll"
        POST_INCLUDE_REGEXES [[.*msvcp140.*\.dll]]
        POST_INCLUDE_REGEXES [[.*vcruntime140.*\.dll]]
        POST_EXCLUDE_REGEXES [[.*system32.*\.dll]]
)
install(DIRECTORY config/templates DESTINATION "." FILES_MATCHING PATTERN "*.png")
install(DIRECTORY config/locales DESTINATION ".")
install(FILES config/actions.json5 config/configuration.json5 config/screens.json5 config/logging.conf DESTINATION ".")
install(FILES EDRobot.cmd.in DESTINATION "." RENAME EDRobot.cmd)

