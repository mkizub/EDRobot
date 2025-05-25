//
// Created by mkizub on 22.05.2025.
//

#include "Keyboard.h"
#include "Utils.h"
#include "easylogging++.h"
#include <map>

namespace keyboard
{
static std::thread interceptor_thread;
static HHOOK keyboardHook;
static DWORD nativeThreadId;
static KeyboardCollbackFn keyboardCallback;

void loop();


//----- Offsets for values in KEYBOARD_MAPPING ---------------------------------
const unsigned int OFFSET_EXTENDEDKEY = 0xE000;
const unsigned int OFFSET_SHIFTKEY    = 0x10000;
//------------------------------------------------------------------------------


// ----- KEYBOARD_MAPPING -------------------------------------------------------
const unsigned int SHIFT_SCANCODE = 0x2A;  // Used in auto-shifting
// should be keyboard MAKE scancodes ( <0x80 )
// some keys require the use of EXTENDEDKEY flags, they
static std::map<std::string, unsigned int> US_QWERTY_MAPPING = {
        { "escape", 0x01 },
        { "esc", 0x01 },
        { "f1", 0x3B },
        { "f2", 0x3C },
        { "f3", 0x3D },
        { "f4", 0x3E },
        { "f5", 0x3F },
        { "f6", 0x40 },
        { "f7", 0x41 },
        { "f8", 0x42 },
        { "f9", 0x43 },
        { "f10", 0x44 },
        { "f11", 0x57 },
        { "f12", 0x58 },
        { "printscreen", 0x54 },  // same result as ScancodeSequence([0xE02A, 0xE037])
        { "prntscrn", 0x54 },     // same result as ScancodeSequence([0xE02A, 0xE037])
        { "prtsc", 0x54 },        // same result as ScancodeSequence([0xE02A, 0xE037])
        { "prtscr", 0x54 },       // same result as ScancodeSequence([0xE02A, 0xE037])
        { "scrolllock", 0x46 },
        { "ctrlbreak", 0x46 + OFFSET_EXTENDEDKEY },
        //{ "pause", ScancodeSequence([0xE11D, 0x45, 0xE19D, 0xC5]) },
        { "`", 0x29 },
        { "1", 0x02 },
        { "2", 0x03 },
        { "3", 0x04 },
        { "4", 0x05 },
        { "5", 0x06 },
        { "6", 0x07 },
        { "7", 0x08 },
        { "8", 0x09 },
        { "9", 0x0A },
        { "0", 0x0B },
        { "-", 0x0C },
        { "=", 0x0D },
        { "~", 0x29 + OFFSET_SHIFTKEY },
        { "!", 0x02 + OFFSET_SHIFTKEY },
        { "@", 0x03 + OFFSET_SHIFTKEY },
        { "#", 0x04 + OFFSET_SHIFTKEY },
        { "$", 0x05 + OFFSET_SHIFTKEY },
        { "%", 0x06 + OFFSET_SHIFTKEY },
        { "^", 0x07 + OFFSET_SHIFTKEY },
        { "&", 0x08 + OFFSET_SHIFTKEY },
        { "*", 0x09 + OFFSET_SHIFTKEY },
        { "(", 0x0A + OFFSET_SHIFTKEY },
        { ")", 0x0B + OFFSET_SHIFTKEY },
        { "_", 0x0C + OFFSET_SHIFTKEY },
        { "+", 0x0D + OFFSET_SHIFTKEY },
        { "backspace", 0x0E },
        { "\b", 0x0E },
        { "insert", 0x52 + OFFSET_EXTENDEDKEY },
        { "home", 0x47 + OFFSET_EXTENDEDKEY },
        { "pageup", 0x49 + OFFSET_EXTENDEDKEY },
        { "pgup", 0x49 + OFFSET_EXTENDEDKEY },
        { "pagedown", 0x51 + OFFSET_EXTENDEDKEY },
        { "pgdn", 0x51 + OFFSET_EXTENDEDKEY },
        // numpad
        { "numlock", 0x45 },
        { "divide", 0x35 + OFFSET_EXTENDEDKEY },
        { "multiply", 0x37 },
        { "subtract", 0x4A },
        { "add", 0x4E },
        { "decimal", 0x53 },
        { "numperiod", 0x53 },
        { "numpadenter", 0x1C + OFFSET_EXTENDEDKEY },
        { "numpad1", 0x4F },
        { "numpad2", 0x50 },
        { "numpad3", 0x51 },
        { "numpad4", 0x4B },
        { "numpad5", 0x4C },
        { "numpad6", 0x4D },
        { "numpad7", 0x47 },
        { "numpad8", 0x48 },
        { "numpad9", 0x49 },
        { "num0", 0x52 },
        { "num1", 0x4F },
        { "num2", 0x50 },
        { "num3", 0x51 },
        { "num4", 0x4B },
        { "num5", 0x4C },
        { "num6", 0x4D },
        { "num7", 0x47 },
        { "num8", 0x48 },
        { "num9", 0x49 },
        { "num0", 0x52 },
        { "clear", 0x4C},  // name from pyautogui
        // end numpad
        { "tab", 0x0F },
        { "\t", 0x0F },
        { "q", 0x10 },
        { "w", 0x11 },
        { "e", 0x12 },
        { "r", 0x13 },
        { "t", 0x14 },
        { "y", 0x15 },
        { "u", 0x16 },
        { "i", 0x17 },
        { "o", 0x18 },
        { "p", 0x19 },
        { "[", 0x1A },
        { "]", 0x1B },
        { "\\", 0x2B },
        { "Q", 0x10 + OFFSET_SHIFTKEY },
        { "W", 0x11 + OFFSET_SHIFTKEY },
        { "E", 0x12 + OFFSET_SHIFTKEY },
        { "R", 0x13 + OFFSET_SHIFTKEY },
        { "T", 0x14 + OFFSET_SHIFTKEY },
        { "Y", 0x15 + OFFSET_SHIFTKEY },
        { "U", 0x16 + OFFSET_SHIFTKEY },
        { "I", 0x17 + OFFSET_SHIFTKEY },
        { "O", 0x18 + OFFSET_SHIFTKEY },
        { "P", 0x19 + OFFSET_SHIFTKEY },
        { "{", 0x1A + OFFSET_SHIFTKEY },
        { "}", 0x1B + OFFSET_SHIFTKEY },
        { "|", 0x2B + OFFSET_SHIFTKEY },
        { "del", 0x53 + OFFSET_EXTENDEDKEY },
        { "delete", 0x53 + OFFSET_EXTENDEDKEY },
        { "end", 0x4F + OFFSET_EXTENDEDKEY },
        { "capslock", 0x3A },
        { "a", 0x1E },
        { "s", 0x1F },
        { "d", 0x20 },
        { "f", 0x21 },
        { "g", 0x22 },
        { "h", 0x23 },
        { "j", 0x24 },
        { "k", 0x25 },
        { "l", 0x26 },
        { ";", 0x27 },
        { "'", 0x28 },
        { "A", 0x1E + OFFSET_SHIFTKEY },
        { "S", 0x1F + OFFSET_SHIFTKEY },
        { "D", 0x20 + OFFSET_SHIFTKEY },
        { "F", 0x21 + OFFSET_SHIFTKEY },
        { "G", 0x22 + OFFSET_SHIFTKEY },
        { "H", 0x23 + OFFSET_SHIFTKEY },
        { "J", 0x24 + OFFSET_SHIFTKEY },
        { "K", 0x25 + OFFSET_SHIFTKEY },
        { "L", 0x26 + OFFSET_SHIFTKEY },
        { ",", 0x27 + OFFSET_SHIFTKEY },
        { "\"", 0x28 + OFFSET_SHIFTKEY },
        { "enter", 0x1C },
        { "return", 0x1C },
        { "\n", 0x1C },
        { "shift", SHIFT_SCANCODE },
        { "shiftleft", SHIFT_SCANCODE },
        { "z", 0x2C },
        { "x", 0x2D },
        { "c", 0x2E },
        { "v", 0x2F },
        { "b", 0x30 },
        { "n", 0x31 },
        { "m", 0x32 },
        { ",", 0x33 },
        { ".", 0x34 },
        { "/", 0x35 },
        { "Z", 0x2C + OFFSET_SHIFTKEY },
        { "X", 0x2D + OFFSET_SHIFTKEY },
        { "C", 0x2E + OFFSET_SHIFTKEY },
        { "V", 0x2F + OFFSET_SHIFTKEY },
        { "B", 0x30 + OFFSET_SHIFTKEY },
        { "N", 0x31 + OFFSET_SHIFTKEY },
        { "M", 0x32 + OFFSET_SHIFTKEY },
        { "<", 0x33 + OFFSET_SHIFTKEY },
        { ">", 0x34 + OFFSET_SHIFTKEY },
        { "?", 0x35 + OFFSET_SHIFTKEY },
        { "shiftright", 0x36 },
        { "ctrl", 0x1D },
        { "ctrlleft", 0x1D },
        { "win", 0x5B + OFFSET_EXTENDEDKEY },
        { "super", 0x5B + OFFSET_EXTENDEDKEY},  // name from pyautogui
        { "winleft", 0x5B + OFFSET_EXTENDEDKEY },
        { "alt", 0x38 },
        { "altleft", 0x38 },
        { " ", 0x39 },
        { "space", 0x39 },
        { "altright", 0x38 + OFFSET_EXTENDEDKEY },
        { "winright", 0x5C + OFFSET_EXTENDEDKEY },
        { "apps", 0x5D + OFFSET_EXTENDEDKEY },
        { "context", 0x5D + OFFSET_EXTENDEDKEY },
        { "contextmenu", 0x5D + OFFSET_EXTENDEDKEY },
        { "ctrlright", 0x1D + OFFSET_EXTENDEDKEY },
        // Originally from learncodebygaming/pydirectinput:
        // arrow key scancodes can be different depending on the hardware,
        // so I think the best solution is to look it up based on the virtual key
        // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mapvirtualkeya
        { "up",    MapVirtualKey(VK_UP,    MAPVK_VK_TO_VSC_EX) | OFFSET_EXTENDEDKEY },
        { "left",  MapVirtualKey(VK_LEFT,  MAPVK_VK_TO_VSC_EX) | OFFSET_EXTENDEDKEY },
        { "down",  MapVirtualKey(VK_DOWN,  MAPVK_VK_TO_VSC_EX) | OFFSET_EXTENDEDKEY },
        { "right", MapVirtualKey(VK_RIGHT, MAPVK_VK_TO_VSC_EX) |  OFFSET_EXTENDEDKEY },
        // While forking the original repository and working on the code },
        // I'm starting to doubt this still holds true.
        // As far as I can see, arrow keys are just the Numpad scancodes for Num
        // 2, 4, 6, and 8 with EXTENDEDKEY flag.
        // In fact, looking up the virtual key codes will just return the very same
        // scancodes the Numpad keys occupy.
        { "help", 0x63 },
        { "sleep", 0x5F + OFFSET_EXTENDEDKEY },
        { "medianext", 0x19 + OFFSET_EXTENDEDKEY },
        { "nexttrack", 0x19 + OFFSET_EXTENDEDKEY },
        { "mediaprevious", 0x10 + OFFSET_EXTENDEDKEY },
        { "prevtrack", 0x10 + OFFSET_EXTENDEDKEY },
        { "mediastop", 0x24 + OFFSET_EXTENDEDKEY },
        { "stop", 0x24 + OFFSET_EXTENDEDKEY },
        { "mediaplay", 0x22 + OFFSET_EXTENDEDKEY },
        { "mediapause", 0x22 + OFFSET_EXTENDEDKEY },
        { "playpause", 0x22 + OFFSET_EXTENDEDKEY },
        { "mute", 0x20 + OFFSET_EXTENDEDKEY },
        { "volumemute", 0x20 + OFFSET_EXTENDEDKEY },
        { "volumeup", 0x30 + OFFSET_EXTENDEDKEY },
        { "volup", 0x30 + OFFSET_EXTENDEDKEY },
        { "volumedown", 0x2E + OFFSET_EXTENDEDKEY },
        { "voldown", 0x2E + OFFSET_EXTENDEDKEY },
        { "media", 0x6D + OFFSET_EXTENDEDKEY },
        { "launchmediaselect", 0x6D + OFFSET_EXTENDEDKEY },
        { "email", 0x6C + OFFSET_EXTENDEDKEY },
        { "launchmail", 0x6C + OFFSET_EXTENDEDKEY },
        { "calculator", 0x21 + OFFSET_EXTENDEDKEY },
        { "calc", 0x21 + OFFSET_EXTENDEDKEY },
        { "launch1", 0x6B + OFFSET_EXTENDEDKEY },
        { "launchapp1", 0x6B + OFFSET_EXTENDEDKEY },
        { "launch2", 0x21 + OFFSET_EXTENDEDKEY },
        { "launchapp2", 0x21 + OFFSET_EXTENDEDKEY },
        { "browsersearch", 0x65 + OFFSET_EXTENDEDKEY },
        { "browserhome", 0x32 + OFFSET_EXTENDEDKEY },
        { "browserforward", 0x69 + OFFSET_EXTENDEDKEY },
        { "browserback", 0x6A + OFFSET_EXTENDEDKEY },
        { "browserstop", 0x68 + OFFSET_EXTENDEDKEY },
        { "browserrefresh", 0x67 + OFFSET_EXTENDEDKEY },
        { "browserfavorites", 0x66 + OFFSET_EXTENDEDKEY },
        { "f13", 0x64 },
        { "f14", 0x65 },
        { "f15", 0x66 },
        { "f16", 0x67 },
        { "f17", 0x68 },
        { "f18", 0x69 },
        { "f19", 0x6A },
        { "f20", 0x6B },
        { "f21", 0x6C },
        { "f22", 0x6D },
        { "f23", 0x6E },
        { "f24", 0x76}
};

void start(KeyboardCollbackFn callback) {
    keyboardCallback = callback;
    interceptor_thread = std::thread(&loop);
}

void stop() {
    if (interceptor_thread.joinable()) {
        if (nativeThreadId) {
            PostThreadMessage(nativeThreadId, WM_QUIT, 0, 0);
            nativeThreadId = 0;
        }
        interceptor_thread.join();
    }
}

bool sendDown(const std::string& nm) {
    auto it = US_QWERTY_MAPPING.find(nm);
    if (it == US_QWERTY_MAPPING.end()) {
        LOG(ERROR) << "Scancode for " << nm << " not found";
        return false;
    }
    unsigned code = it->second & 0xFFFF;
    bool extended = (code & OFFSET_EXTENDEDKEY) != 0;
    LOG(INFO) << "SendInput   key down '" << nm << "' " << (code & 0xFF) << " success";
    INPUT input[1]{};
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wScan = code;
    input[0].ki.dwFlags = KEYEVENTF_SCANCODE;
    if (extended)
        input[0].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    unsigned sent = SendInput(1, input, sizeof(input));
    if (!sent) {
        LOG(ERROR) << "SendInput keydown '" << nm << "' " << code << " failed: " << getErrorMessage();
        return false;
    }
    return true;
}

bool sendUp(const std::string& nm) {
    auto it = US_QWERTY_MAPPING.find(nm);
    if (it == US_QWERTY_MAPPING.end()) {
        LOG(ERROR) << "Scancode for " << nm << " not found";
        return false;
    }
    unsigned code = it->second & 0xFFFF;
    bool extended = (code & OFFSET_EXTENDEDKEY) >= OFFSET_EXTENDEDKEY;
    LOG(INFO) << "SendInput   key up   '" << nm << "' " << (code & 0xFF) << " success";
    INPUT input[1]{};
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wScan = code;
    input[0].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    if (extended)
        input[0].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    unsigned sent = SendInput(1, input, sizeof(input));
    if (!sent) {
        LOG(ERROR) << "SendInput keyup   '" << nm << "' " << code << " failed: " << getErrorMessage();
        return false;
    }
    return true;
}


LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        auto* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            //LOG(DEBUG) << "intercepted key down: code " << pKeyBoard->vkCode << " scancode " << pKeyBoard->scanCode << " flags " << pKeyBoard->flags;
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            auto code = pKeyBoard->vkCode;
            if (code == VK_ESCAPE || code == VK_SNAPSHOT || code == VK_PAUSE) {
                //LOG(DEBUG) << "intercepted key up  : code " << pKeyBoard->vkCode << " scancode " << pKeyBoard->scanCode << " flags " << pKeyBoard->flags;
                int flags = 0;
                if (GetKeyState(VK_SHIFT) & 0x8000) flags |= SHIFT;
                if (GetKeyState(VK_CONTROL) & 0x8000) flags |= CTRL;
                if (GetKeyState(VK_MENU) & 0x8000) flags |= ALT;
                if ((GetKeyState(VK_LWIN)| GetKeyState(VK_RWIN)) & 0x8000) flags |= WIN;
                keyboardCallback(pKeyBoard->vkCode, pKeyBoard->scanCode, flags);
            }
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

void loop() {
    nativeThreadId = GetCurrentThreadId();
    HINSTANCE hInstance = GetModuleHandle(NULL);
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    if (keyboardHook == nullptr) {
        LOG(ERROR) << "Failed to set hook: " << getErrorMessage();
        nativeThreadId = 0;
        return;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(keyboardHook);
    nativeThreadId = 0;
}

} // namespace keyboard
