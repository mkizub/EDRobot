//
// Created by mkizub on 22.05.2025.
//

#include "pch.h"

#include "Keyboard.h"

namespace keyboard
{
static std::thread interceptor_thread;
static HHOOK keyboardHook;
static DWORD nativeThreadId;
static KeyboardCollbackFn keyboardCallback;

void loop();


//----- Offsets for values in KEYBOARD_MAPPING ---------------------------------
const unsigned int EXT_KEY   = 0xE000;
const unsigned int SHIFT_KEY = 0x10000;
//------------------------------------------------------------------------------


// ----- KEYBOARD_MAPPING -------------------------------------------------------
const unsigned int SHIFT_SCANCODE = 0x2A;  // Used in auto-shifting

struct Key {
    DWORD vkCode;
    DWORD scanCode;
    std::vector<std::string> names;
    Key(char ch, DWORD sc) {
        vkCode = ch & 0xFFU;
        scanCode = sc;
        char buf[]{ch,0};
        names.emplace_back(buf);
    }
    Key(DWORD vk, DWORD sc, const char* name) {
        vkCode = vk;
        scanCode = sc;
        names.emplace_back(name);
    }
    Key(DWORD vk, DWORD sc, std::vector<std::string> aliases) {
        vkCode = vk;
        scanCode = sc;
        names = aliases;
    }
};
static Key US_QWERTY_KEYBOARD_TABLE[] = {
        { VK_ESCAPE, 0x01, {"Esc", "Escape", "\033"} },
        { VK_F1,     0x3b, "F1" },
        { VK_F2,     0x3C, "F2" },
        { VK_F3,     0x3D, "F3" },
        { VK_F4,     0x3E, "F4" },
        { VK_F5,     0x3F, "F5" },
        { VK_F6,     0x40, "F6" },
        { VK_F7,     0x41, "F7" },
        { VK_F8,     0x42, "F8" },
        { VK_F9,     0x43, "F9" },
        { VK_F10,    0x44, "F10" },
        { VK_F11,    0x57, "F11" },
        { VK_F12,            0x58,           "F12" },
        { VK_SNAPSHOT,       0x54,           {"PrintScreen", "PrntScrn", "PrtScr", "PrtSc", "Snapshot"} },
        { VK_SCROLL,         0x46,           {"ScrollLock", "Scroll"} },
        { VK_F1,             0x46 | EXT_KEY, "CtrlBreak" },
        //{ VK_PAUSE,          0    | EXT_KEY, "Pause"     }, //ScancodeSequence([0xE11D, 0x45, 0xE19D, 0xC5])
        { VK_OEM_3,          0x29,           "`" },
        { '1',               0x02 },
        { '2',               0x03 },
        { '3',               0x04 },
        { '4',               0x05 },
        { '5',               0x06 },
        { '6',               0x07 },
        { '7',               0x08 },
        { '8',               0x09 },
        { '9',               0x0A },
        { '0',               0x0B },
        { '-',               0x0C },
        { '=',               0x02 },
        { '~',               0x29 | EXT_KEY },
        { '!',               0x02 | EXT_KEY },
        { '@',               0x03 | EXT_KEY },
        { '#',               0x04 | EXT_KEY },
        { '$',               0x05 | EXT_KEY },
        { '%',               0x06 | EXT_KEY },
        { '^',               0x07 | EXT_KEY },
        { '&',               0x08 | EXT_KEY },
        { '*',               0x09 | EXT_KEY },
        { '(',               0x0A | EXT_KEY },
        { ')',               0x0B | EXT_KEY },
        { '_',               0x0C | EXT_KEY },
        { '+',               0x0D | EXT_KEY },
        { VK_BACK,           0x0E,           {"BackSpace", "\b" } },
        { VK_INSERT,         0x52 | EXT_KEY, "Insert" },
        { VK_HOME,           0x47 | EXT_KEY, "Home" },
        { VK_PRIOR,          0x49 | EXT_KEY, {"PgUp", "PageUp" } },
        { VK_NEXT,           0x51 | EXT_KEY, {"PgDn", "PageDown" } },
        // numpad
        { VK_NUMLOCK,        0x45,           "NumLock" },
        { VK_DIVIDE,         0x35 | EXT_KEY, "Divide" },
        { VK_MULTIPLY,       0x37,           "Multiply" },
        { VK_SUBTRACT,       0x4A,           "Subtract" },
        { VK_ADD,            0x4E,           "Add" },
        { VK_DECIMAL,        0x53,           {"Decimal","NumPeriod", "NumDel"} },
        { VK_RETURN,         0x53 | EXT_KEY, {"NumpadEnter", "\r"} },
        { VK_NUMPAD1,        0x4F,           {"Num1","Numpad1", "NumEnd"} },
        { VK_NUMPAD2,        0x50,           {"Num2","Numpad3", "NumDown"} },
        { VK_NUMPAD3,        0x51,           {"Num3","Numpad4", "NumPgDn"} },
        { VK_NUMPAD4,        0x4B,           {"Num4","Numpad5", "NumLeft"} },
        { VK_NUMPAD5,        0x4C,           {"Num5","Numpad6"} },
        { VK_NUMPAD6,        0x4D,           {"Num6","Numpad7", "NumRight"} },
        { VK_NUMPAD7,        0x47,           {"Num7","Numpad8", "NumHome"} },
        { VK_NUMPAD8,        0x48,           {"Num8","Numpad9", "NumUp"} },
        { VK_NUMPAD9,        0x49,           {"Num9","Numpad1", "NumPgUp"} },
        { VK_NUMPAD0,        0x52,           {"Num0","Numpad0", "NumIns"} },
        // end numpad
        { VK_TAB,            0x0F,           {"Tab","\t"} },
        { 'q',               0x10 },
        { 'w',               0x11 },
        { 'e',               0x12 },
        { 'r',               0x13 },
        { 't',               0x14 },
        { 'y',               0x15 },
        { 'u',               0x16 },
        { 'i',               0x17 },
        { 'o',               0x18 },
        { 'p',               0x19 },
        { '[',               0x1A },
        { ']',               0x1B },
        { '\\',              0x2B },
        { 'Q',               0x10 | SHIFT_KEY },
        { 'W',               0x11 | SHIFT_KEY },
        { 'E',               0x12 | SHIFT_KEY },
        { 'R',               0x13 | SHIFT_KEY },
        { 'T',               0x14 | SHIFT_KEY },
        { 'Y',               0x15 | SHIFT_KEY },
        { 'U',               0x16 | SHIFT_KEY },
        { 'I',               0x17 | SHIFT_KEY },
        { 'O',               0x18 | SHIFT_KEY },
        { 'P',               0x19 | SHIFT_KEY },
        { '{',               0x1A | SHIFT_KEY },
        { '}',               0x1B | SHIFT_KEY },
        { '|',               0x2B | SHIFT_KEY },
        { VK_DELETE,         0x53 | EXT_KEY,  { "Del", "Delete", "\127" } },
        { VK_END,            0x4F | EXT_KEY,  "End" },
        { VK_CAPITAL,        0x3A,            "CapsLock" },
        { 'a',               0x1E },
        { 's',               0x1F },
        { 'd',               0x20 },
        { 'f',               0x21 },
        { 'g',               0x22 },
        { 'h',               0x23 },
        { 'j',               0x24 },
        { 'k',               0x25 },
        { 'l',               0x26 },
        { ';',               0x27 },
        { '\'',              0x28 },
        { 'A',               0x1E | SHIFT_KEY },
        { 'S',               0x1F | SHIFT_KEY },
        { 'D',               0x20 | SHIFT_KEY },
        { 'F',               0x21 | SHIFT_KEY },
        { 'G',               0x22 | SHIFT_KEY },
        { 'H',               0x23 | SHIFT_KEY },
        { 'J',               0x24 | SHIFT_KEY },
        { 'K',               0x25 | SHIFT_KEY },
        { 'L',               0x26 | SHIFT_KEY },
        { ',',               0x27 | SHIFT_KEY },
        { '\"',              0x28 | SHIFT_KEY },
        { VK_RETURN,         0x1C,            { "Enter", "Return", "\n" } },
        { VK_SHIFT,          0x2A,            { "ShiftLeft", "Shift" } },
        { 'z',               0x2C },
        { 'x',               0x2D },
        { 'c',               0x2E },
        { 'v',               0x2F },
        { 'b',               0x30 },
        { 'n',               0x31 },
        { 'm',               0x32 },
        { ',',               0x33 },
        { '.',               0x34 },
        { '/',               0x35 },
        { 'Z',               0x2C | SHIFT_KEY },
        { 'X',               0x2D | SHIFT_KEY },
        { 'C',               0x2E | SHIFT_KEY },
        { 'V',               0x2F | SHIFT_KEY },
        { 'B',               0x30 | SHIFT_KEY },
        { 'N',               0x31 | SHIFT_KEY },
        { 'M',               0x32 | SHIFT_KEY },
        { '<',               0x33 | SHIFT_KEY },
        { '>',               0x34 | SHIFT_KEY },
        { '?',               0x35 | SHIFT_KEY },
        { VK_SHIFT,          0x36,            { "ShiftRight", "Shift" } },
        { VK_CONTROL,        0x1D,            { "CtrlLeft", "Ctrl" } },
        { VK_LWIN,           0x5B | EXT_KEY,  { "LWin", "WinLeft", "Win", "Meta" } },
        { VK_MENU,           0x38,            { "AltLeft", "Alt"} },
        { ' ',               0x39,            { "Space", " "} },
        { VK_MENU,           0x38 | EXT_KEY,  { "AltRight", "Alt"} },
        { VK_RWIN,           0x5C | EXT_KEY,  { "RWin", "WinRight", "Win", "Meta" } },
        { VK_APPS,           0x5D | EXT_KEY,  { "Apps", "ContextMenu", "Context"} },
        { VK_CONTROL,        0x1D | EXT_KEY,  { "CtrlRight", "Ctrl" } },
        { VK_UP,             0x47 | EXT_KEY, "Up"   },
        { VK_LEFT,           0x4B | EXT_KEY, "Left" },
        { VK_DOWN,           0x50 | EXT_KEY, "Down" },
        { VK_RIGHT,          0x4D | EXT_KEY, "Right"},
        { VK_HELP,           0x63,           "Help" },
        { VK_SLEEP,          0x5F | EXT_KEY, "Sleep" },
        { VK_MEDIA_NEXT_TRACK,    0x19 | EXT_KEY, {"MediaNext","NextTrack"} },
        { VK_MEDIA_PREV_TRACK,    0x10 | EXT_KEY, {"MediaPrev","PrevTrack"} },
        { VK_MEDIA_STOP,          0x24 | EXT_KEY, {"MediaStop","Stop"} },
        { VK_MEDIA_PLAY_PAUSE,    0x22 | EXT_KEY, {"MediaPlay","MediaPause","PlayPause"} },
        { VK_VOLUME_MUTE,         0x20 | EXT_KEY, {"VolumeMute","Mute"} },
        { VK_VOLUME_UP,           0x30 | EXT_KEY, {"VolumeUp","VolUp"} },
        { VK_VOLUME_DOWN,         0x2E | EXT_KEY, {"VolumeDown","VolDown"} },
        { VK_LAUNCH_MEDIA_SELECT, 0x6D | EXT_KEY, {"LaunchMediaSelect","Media"} },
        { VK_LAUNCH_MAIL,         0x6C | EXT_KEY, {"LaunchMail","EMail"} },
        //{ "calculator",         0x21 + EXT_KEY },
        { VK_LAUNCH_APP1,         0x6B | EXT_KEY, {"LaunchApp1","Launch1"} },
        { VK_LAUNCH_APP2,         0x21 | EXT_KEY, {"LaunchApp2","Launch2"} },
        { VK_BROWSER_SEARCH,      0x65 | EXT_KEY, "BrowserSearch" },
        { VK_BROWSER_HOME,        0x32 | EXT_KEY, "BrowserHome" },
        { VK_BROWSER_FORWARD,     0x69 | EXT_KEY, "BrowserForward" },
        { VK_BROWSER_BACK,        0x6A | EXT_KEY, "BrowserBack" },
        { VK_BROWSER_STOP,        0x68 | EXT_KEY, "BrowserStop" },
        { VK_BROWSER_REFRESH,     0x67 | EXT_KEY, "BrowserRefresh" },
        { VK_BROWSER_FAVORITES,   0x66 | EXT_KEY, "BrowserFavorites" },
        { VK_F13,     0x64, "F13" },
        { VK_F14,     0x65, "F14" },
        { VK_F15,     0x66, "F15" },
        { VK_F16,     0x67, "F16" },
        { VK_F17,     0x68, "F17" },
        { VK_F18,     0x69, "F18" },
        { VK_F19,     0x6A, "F19" },
        { VK_F20,     0x6B, "F20" },
        { VK_F21,     0x6C, "F21" },
        { VK_F22,     0x6D, "F22" },
        { VK_F23,     0x6E, "F23" },
        { VK_F24,     0x76, "F24" },
};

static std::unordered_map<unsigned int, const Key&> US_QWERTY_MAPPING_SC_TO_KEY;
static std::unordered_map<unsigned int, const Key&> US_QWERTY_MAPPING_VK_TO_NAME;
static std::unordered_map<std::string, const Key&> US_QWERTY_MAPPING_NAME_TO_KEY;

static std::string makeKeyboardMapping() {
    for (const Key& key :  US_QWERTY_KEYBOARD_TABLE) {
        US_QWERTY_MAPPING_SC_TO_KEY.try_emplace(key.scanCode, key);
        US_QWERTY_MAPPING_VK_TO_NAME.try_emplace(key.vkCode, key);
        for (std::string alias : key.names) {
            if (alias.size() == 1) {
                US_QWERTY_MAPPING_NAME_TO_KEY.try_emplace(alias, key);
            } else {
                US_QWERTY_MAPPING_NAME_TO_KEY.try_emplace(toLower(alias), key);
            }
        }
    }
    return "unknown";
}

static std::string unknownKeyName = makeKeyboardMapping();

static std::unordered_map<unsigned int, const Key&> INTERCEPT_VK_KEY_SET;

static void addInterceptKey(const std::string name) {
    auto it = US_QWERTY_MAPPING_NAME_TO_KEY.find(name);
    if (it == US_QWERTY_MAPPING_NAME_TO_KEY.end()) {
        LOG(ERROR) << "Key '" << name << "' (requested to be intercepted) not found";
        return;
    }
    const Key& k = it->second;
    INTERCEPT_VK_KEY_SET.try_emplace(k.vkCode, k);
}

void intercept(const std::vector<std::string>& keys) {
    INTERCEPT_VK_KEY_SET.clear();
    for (const std::string& nm : keys) {
        std::string name = toLower(nm);
        if (name.size() == 1 && isLatinLetter(name[0])) {
            addInterceptKey(name);
            addInterceptKey(toUpper(name));
            continue;
        }
        addInterceptKey(name);
    }
}

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

bool sendKeyDown(const std::string& nm) {
    auto it = US_QWERTY_MAPPING_NAME_TO_KEY.find(toLower(nm));
    if (it == US_QWERTY_MAPPING_NAME_TO_KEY.end()) {
        LOG(ERROR) << "Scancode for " << nm << " not found";
        return false;
    }
    const Key& key = it->second;
    unsigned code = key.scanCode & 0xFFFF;
    bool extended = (code & EXT_KEY) != 0;
    LOG(DEBUG) << "SendInput   key down '" << nm << "' " << (code & 0xFF);
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

bool sendKeyUp(const std::string& nm) {
    auto it = US_QWERTY_MAPPING_NAME_TO_KEY.find(toLower(nm));
    if (it == US_QWERTY_MAPPING_NAME_TO_KEY.end()) {
        LOG(ERROR) << "Scancode for " << nm << " not found";
        return false;
    }
    const Key& key = it->second;
    unsigned code = key.scanCode & 0xFFFF;
    bool extended = (code & EXT_KEY) >= EXT_KEY;
    LOG(DEBUG) << "SendInput   key up   '" << nm << "' " << (code & 0xFF);
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

bool sendMouseMoveTo(int x, int y, bool absolute) {
    DWORD flags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
    if (absolute) {
        flags |= MOUSEEVENTF_ABSOLUTE;
        // TODO: fix this for windowed mode and multiple monitors, see https://stackoverflow.com/questions/62759122/calculate-normalized-coordinates-for-sendinput-in-a-multi-monitor-environment
        x = MulDiv(x, 65536, GetSystemMetrics(SM_CXSCREEN));
        y = MulDiv(y, 65536, GetSystemMetrics(SM_CYSCREEN));
    }
    INPUT input[1]{};
    input[0].type = INPUT_MOUSE;
    input[0].mi.dx = x;
    input[0].mi.dy = y;
    input[0].mi.dwFlags = flags;
    unsigned sent = SendInput(1, input, sizeof(input));
    if (!sent) {
        LOG(ERROR) << "SendInput mouse move dx:" << x << ", dy:" << y << " failed: " << getErrorMessage();
        return false;
    }
    return true;
}

bool sendMouseDown(int buttons) {
    DWORD flags = 0;
    if (buttons & MOUSE_L_BUTTON)
        flags |= MOUSEEVENTF_LEFTDOWN;
    if (buttons & MOUSE_R_BUTTON)
        flags |= MOUSEEVENTF_RIGHTDOWN;
    if (buttons & MOUSE_M_BUTTON)
        flags |= MOUSEEVENTF_MIDDLEDOWN;
    INPUT input[1]{};
    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = flags;
    unsigned sent = SendInput(1, input, sizeof(input));
    if (!sent) {
        LOG(ERROR) << "SendInput mouse down " << flags << " failed: " << getErrorMessage();
        return false;
    }
    return true;
}
bool sendMouseUp(int buttons) {
    DWORD flags = 0;
    if (buttons & MOUSE_L_BUTTON)
        flags |= MOUSEEVENTF_LEFTUP;
    if (buttons & MOUSE_R_BUTTON)
        flags |= MOUSEEVENTF_RIGHTUP;
    if (buttons & MOUSE_M_BUTTON)
        flags |= MOUSEEVENTF_MIDDLEUP;
    INPUT input[1]{};
    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = flags;
    unsigned sent = SendInput(1, input, sizeof(input));
    if (!sent) {
        LOG(ERROR) << "SendInput mouse up " << flags << " failed: " << getErrorMessage();
        return false;
    }
    return true;
}
bool sendMouseWheel(int count) {
    DWORD flags = MOUSEEVENTF_WHEEL;
    INPUT input[1]{};
    input[0].type = INPUT_MOUSE;
    input[0].mi.mouseData = count * WHEEL_DELTA;
    input[0].mi.dwFlags = flags;
    unsigned sent = SendInput(1, input, sizeof(input));
    if (!sent) {
        LOG(ERROR) << "SendInput mouse wheel " << count << " failed: " << getErrorMessage();
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
            auto vkCode = pKeyBoard->vkCode;
            if (INTERCEPT_VK_KEY_SET.contains(vkCode)) {
                //LOG(DEBUG) << "intercepted key up  : code " << pKeyBoard->vkCode << " scancode " << pKeyBoard->scanCode << " flags " << pKeyBoard->flags;
                int flags = 0;
                if (GetKeyState(VK_SHIFT) & 0x8000) flags |= SHIFT;
                if (GetKeyState(VK_CONTROL) & 0x8000) flags |= CTRL;
                if (GetKeyState(VK_MENU) & 0x8000) flags |= ALT;
                if ((GetKeyState(VK_LWIN)| GetKeyState(VK_RWIN)) & 0x8000) flags |= WIN;
                std::string keyName;
                const auto& it = US_QWERTY_MAPPING_VK_TO_NAME.find(vkCode);
                if (it != US_QWERTY_MAPPING_SC_TO_KEY.end())
                    keyName = it->second.names[0];
                else
                    keyName = unknownKeyName;
                keyboardCallback(pKeyBoard->vkCode, pKeyBoard->scanCode, flags, keyName);
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
