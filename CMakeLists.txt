cmake_minimum_required(VERSION 3.28)

set (CMAKE_CONFIGURATION_TYPES Debug Release)

project(EDRobot VERSION 0.8.4)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS TRUE)

set(OpenCV_ROOT "${VCPKG_INSTALLED_DIR}/x64-windows/share/opencv4")
find_package(OpenCV REQUIRED)

#find_package(Gettext REQUIRED)
find_library(GETTEXT_LIBRARIES intl iconv)
find_package(CURL REQUIRED)
find_package(cpptrace CONFIG REQUIRED)
add_library(vJoyInterface SHARED IMPORTED)
set_property(TARGET vJoyInterface PROPERTY IMPORTED_LOCATION_RELEASE "${CMAKE_SOURCE_DIR}/thirdparty/vjoy/lib/x64/vJoyInterface.dll")
set_property(TARGET vJoyInterface PROPERTY IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/thirdparty/vjoy/lib/x64/vJoyInterface.dll")
set_property(TARGET vJoyInterface PROPERTY IMPORTED_IMPLIB "${CMAKE_SOURCE_DIR}/thirdparty/vjoy/lib/x64/vJoyInterface.lib")

find_package(Tesseract CONFIG REQUIRED)
#set_target_properties(leptonica PROPERTIES MAP_IMPORTED_CONFIG_DEBUG "Release")
#set_target_properties(Tesseract::libtesseract PROPERTIES MAP_IMPORTED_CONFIG_DEBUG "Release")

add_compile_definitions(UNICODE)
add_compile_definitions(NOMINMAX)
add_compile_definitions(ELPP_NO_DEFAULT_LOG_FILE ELPP_THREAD_SAFE)
add_compile_options(/Zc:__cplusplus /await:strict /utf-8 /Zi)
add_link_options(/DEBUG /PDBCOMPRESS)

add_executable(EDRobot
        EDRobot.manifest
        src/pch.cpp
        src/pch.h
        src/ed_opencv.h
        src/Types.h
        src/main.cpp
        src/ClassifyEnv.cpp
        src/ClassifyEnv.h
        src/Configuration.cpp
        src/EventParser.cpp
        src/Configuration.h
        src/EDWidget.cpp
        src/EDWidget.h
        src/FuzzyMatch.cpp
        src/FuzzyMatch.h
        src/Galaxy.cpp
        src/Galaxy.h
        src/GalaxyNav.cpp
        src/Keyboard.cpp
        src/Keyboard.h
        src/Master.cpp
        src/Master.h
        src/OCR.cpp
        src/OCR.h
        src/ShipStats.cpp
        src/ShipStats.h
        src/State.h
        src/Utils.cpp
        src/Utils.h

        src/ai/AIManager.cpp
        src/ai/AIManager.h
        src/ai/AIUtils.cpp
        src/ai/AIUtils.h
        src/ai/AutopilotTasks.cpp
        src/ai/AutopilotTasks.h
        src/ai/NavList.cpp
        src/ai/NavList.h
        src/ai/ShipTracker.cpp
        src/ai/Task.cpp
        src/ai/Task.h
        src/ai/TaskTemplate.cpp
        src/ai/TaskTemplate.h
        src/ai/TaskDebug.cpp
        src/ai/TaskDebug.h
        src/ai/TradeTasks.cpp
        src/ai/TradeTasks.h
        src/ai/Types.h

        src/detect/Detector.h
        src/detect/Detector.cpp
        src/detect/ImageTemplate.cpp
        src/detect/Lines.cpp
        src/detect/Lines.h
        src/detect/Compass.cpp
        src/detect/NavPanel.cpp
        src/detect/NavPanel.h
        src/detect/Tiles.cpp

        src/ui/UIAddTask.cpp
        src/ui/UIAddTask.h
        src/ui/UIMainDialog.cpp
        src/ui/UIMainDialog.h
        src/ui/UIDebug.cpp
        src/ui/UIDebug.h
        src/ui/UIDialog.cpp
        src/ui/UIDialog.h
        src/ui/UILayout.h
        src/ui/UIManager.cpp
        src/ui/UIManager.h
        src/ui/UISelectRect.cpp
        src/ui/UISelectRect.h
        src/ui/UIStartupDialog.cpp
        src/ui/UIStartupDialog.h
        src/ui/UITaskEditor.cpp
        src/ui/UITaskEditor.h
        src/ui/UIToast.cpp
        src/ui/UIToast.h
        src/ui/UIWindow.cpp
        src/ui/UIWindow.h

        src/Capturer.h
        src/capture/Capturer.cpp
        src/capture/CapturerWin32.cpp
        src/capture/CapturerWin32.h
        src/capture/CapturerDXGI.cpp
        src/capture/CapturerDXGI.h
        src/capture/CaptureWinRT.cpp
        src/capture/CapturerWinRT.h

        ui/Dialogs.rc

        thirdparty/easylogging/easylogging++.cc
        thirdparty/easylogging/easylogging++.h
        thirdparty/dirlistener/ReadDirectoryChanges.cpp
        thirdparty/dirlistener/ReadDirectoryChanges.h
        thirdparty/dirlistener/ReadDirectoryChangesPrivate.cpp
        thirdparty/dirlistener/ReadDirectoryChangesPrivate.h
        thirdparty/dirlistener/ThreadSafeQueue.h
        thirdparty/CLI11/CLI11.hpp
        thirdparty/glob/glob.cpp
        thirdparty/glob/glob.h
        thirdparty/vjoy/vjoyinterface.h
        thirdparty/xml/xml.h
)


target_precompile_headers(EDRobot PUBLIC src/pch.h)
target_compile_definitions(EDRobot PUBLIC "EDROBOT_VERSION=\"${PROJECT_VERSION}\"" )
target_include_directories(EDRobot PRIVATE ${OpenCV_INCLUDE_DIRS})
target_include_directories(EDRobot PRIVATE "${VCPKG_INSTALLED_DIR}/x64-windows/include")
target_include_directories(EDRobot PRIVATE thirdparty)

target_link_libraries(EDRobot PRIVATE ${GETTEXT_LIBRARIES})
target_link_libraries(EDRobot PRIVATE ${OpenCV_LIBS})
target_link_libraries(EDRobot PRIVATE Tesseract::libtesseract)
target_link_libraries(EDRobot PRIVATE CURL::libcurl)
target_link_libraries(EDRobot PRIVATE dxgi d3d11 winmm shcore dwmapi d2d1 windowsapp)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(EDRobot PRIVATE cpptrace::cpptrace)
endif()

function(copy_target_dll target)
  get_target_property(LocationOfDLL ${target} IMPORTED_LOCATION_RELEASE)
  add_custom_command(TARGET EDRobot POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${LocationOfDLL}"
          "$<TARGET_FILE_DIR:EDRobot>"
  )
endfunction()
function(copy_release_dll dll)
  add_custom_command(TARGET EDRobot POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${VCPKG_INSTALLED_DIR}/x64-windows/bin/${dll}.dll"
          "$<TARGET_FILE_DIR:EDRobot>"
  )
endfunction()

target_link_libraries(EDRobot PRIVATE vJoyInterface)
copy_target_dll(vJoyInterface)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  copy_target_dll(leptonica)
  copy_target_dll(Tesseract::libtesseract)
  copy_release_dll(libcurl)
  copy_release_dll(tiff)
  copy_release_dll(libpng16)
  copy_release_dll(zlib1)
endif()

install(TARGETS EDRobot RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-win-.*\.dll" "ext-ms-.*\.dll"
        POST_INCLUDE_REGEXES [[.*msvcp140.*\.dll]]
        POST_INCLUDE_REGEXES [[.*vcruntime140.*\.dll]]
        POST_EXCLUDE_REGEXES [[.*system32.*\.dll]]
)
install(DIRECTORY config/templates DESTINATION "." FILES_MATCHING PATTERN "*.png")
install(DIRECTORY config/locales DESTINATION ".")
install(DIRECTORY config/tessdata DESTINATION ".")
install(DIRECTORY config/ControlSchemes DESTINATION ".")
install(FILES
        config/eddb.json5
        config/commodity-database.json5
        config/configuration.json5
        config/screens.json5
        config/logging.conf
        DESTINATION ".")
install(FILES EDRobot.cmd.in DESTINATION "." RENAME EDRobot.cmd)
install(FILES README.md DESTINATION ".")

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "EDRobot-${PROJECT_VERSION}")
include(CPack)
