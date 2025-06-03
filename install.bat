set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

%CMAKE%  --install cmake-build\msvc\Debug --config Debug --prefix "D:\Work\ED\tmp\dist\EDRobotDebug"
%CMAKE%  --install cmake-build\msvc\Release --config Release --prefix "D:\Work\ED\tmp\dist\EDRobot"
