cmake_minimum_required(VERSION 3.26)

set (CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo)

project(EDRobot)

set(CMAKE_CXX_STANDARD 23)
set(BUILD_SHARED_LIBS TRUE)

set(OpenCV_ROOT "${VCPKG_INSTALLED_DIR}/x64-windows/share/opencv4")
find_package(OpenCV REQUIRED)
#find_package(Gettext REQUIRED)
find_library(GETTEXT_LIBRARIES intl iconv)


#add_compile_definitions(UNICODE)
add_compile_definitions(NOMINMAX)
add_compile_definitions(ELPP_NO_DEFAULT_LOG_FILE)
if (MSVC)
    add_compile_options(-Zc:__cplusplus)
endif()

find_package(cpptrace CONFIG REQUIRED)

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
#        wintoastlib.cpp
#        wintoastlib.h
        Configuration.cpp
        Configuration.h
        dirlistener/ReadDirectoryChanges.cpp
        dirlistener/ReadDirectoryChanges.h
        dirlistener/ReadDirectoryChangesPrivate.cpp
        dirlistener/ReadDirectoryChangesPrivate.h
        dirlistener/ThreadSafeQueue.h
)
#target_precompile_headers(EDRobot PUBLIC pch.h)
target_include_directories(EDRobot PRIVATE ${OpenCV_INCLUDE_DIRS})
target_include_directories(EDRobot PRIVATE "${VCPKG_INSTALLED_DIR}/x64-windows/include")
#target_include_directories(EDRobot PRIVATE ${GETTEXT_INCLUDE_DIR})
target_link_libraries(EDRobot ${GETTEXT_LIBRARIES})
target_link_libraries(EDRobot ${OpenCV_LIBS} winmm shcore) # cpptrace::cpptrace msimg32
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(EDRobot cpptrace::cpptrace)
endif()

install(TARGETS EDRobot RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-win-.*\.dll" "ext-ms-.*\.dll"
        POST_INCLUDE_REGEXES [[.*msvcp140.*\.dll]]
        POST_INCLUDE_REGEXES [[.*vcruntime140.*\.dll]]
        POST_EXCLUDE_REGEXES [[.*system32.*\.dll]]
)
install(DIRECTORY templates DESTINATION "." FILES_MATCHING PATTERN "*.png")
install(DIRECTORY locales DESTINATION ".")
install(FILES actions.json5 configuration.json5 screens.json5 logging.conf DESTINATION ".")
install(FILES EDRobot.cmd.in DESTINATION "." RENAME EDRobot.cmd)

