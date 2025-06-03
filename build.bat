setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

set VCPKG_ROOT=D:\Work\vcpkg
set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

%CMAKE% -DCMAKE_BUILD_TYPE=Debug -D "CMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
        -G Ninja -S . -B cmake-build\msvc\Debug
%CMAKE% --build cmake-build\msvc\Debug

%CMAKE% -DCMAKE_BUILD_TYPE=Release -D "CMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
        -G Ninja -S . -B cmake-build\msvc\Release
%CMAKE% --build cmake-build\msvc\Release

endlocal