//
// Created by mkizub on 23.05.2025.
//

#include "Utils.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

std::string getErrorMessage() {
    return getErrorMessage(GetLastError());
}
std::string getErrorMessage(unsigned errorCode) {
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            (LPSTR)&messageBuffer,
            0,
            nullptr
    );

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}

