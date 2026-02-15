//
// Created by mkizub on 22.05.2025.
//

#include "pch.h"
#include <ranges>

#include "Keyboard.h"
#include "vjoy/vjoyinterface.h"
#include "Capturer.h"

namespace ai {
void check_interrupted();
}

namespace kbd
{
static bool keyboardShutdown;
static std::thread interceptor_thread;
static HHOOK keyboardHook;
static DWORD nativeThreadId;
static HANDLE hKeyboardEvent;
static std::mutex keyboardMutex;
static KeyboardCollbackFn keyboardCallback;
static std::atomic<unsigned> keyboardInputCounter(1);

void loop();

static bool vJoyAcquired;
static std::map<std::string,vJoyAxisInfo> vJoyAxisMap;


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
        if ((sc & EXT_KEY) == 0)
            vkCode = ch & 0xFFU;
        else
            vkCode = 0;
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
        { VK_PAUSE,          0x45,           {"Pause","Break"} },
        { VK_OEM_3,          0x29,           {"`", "Grave"} },
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
        { '-',               0x0C,          {"-", "Minus" } },
        { '=',               0x02,          {"=", "Equals" } },
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
        { VK_DIVIDE,         0x35 | EXT_KEY, {"Divide", "Numpad_Divide"} },
        { VK_MULTIPLY,       0x37,           {"Multiply", "Numpad_Multiply"} },
        { VK_SUBTRACT,       0x4A,           {"Subtract", "Numpad_Subtract"} },
        { VK_ADD,            0x4E,           {"Add", "Numpad_Add"} },
        { VK_DECIMAL,        0x53,           {"Decimal","NumPeriod", "NumDel", "Numpad_Decimal"} },
        { VK_RETURN,         0x53 | EXT_KEY, {"NumpadEnter", "Numpad_Enter", "\r"} },
        { VK_NUMPAD1,        0x4F,           {"Num1","Numpad1", "NumEnd", "Numpad_1"} },
        { VK_NUMPAD2,        0x50,           {"Num2","Numpad3", "NumDown", "Numpad_2"} },
        { VK_NUMPAD3,        0x51,           {"Num3","Numpad4", "NumPgDn", "Numpad_3"} },
        { VK_NUMPAD4,        0x4B,           {"Num4","Numpad5", "NumLeft", "Numpad_4"} },
        { VK_NUMPAD5,        0x4C,           {"Num5","Numpad6", "Numpad_5"} },
        { VK_NUMPAD6,        0x4D,           {"Num6","Numpad7", "NumRight", "Numpad_6"} },
        { VK_NUMPAD7,        0x47,           {"Num7","Numpad8", "NumHome", "Numpad_7"} },
        { VK_NUMPAD8,        0x48,           {"Num8","Numpad9", "NumUp", "Numpad_8"} },
        { VK_NUMPAD9,        0x49,           {"Num9","Numpad1", "NumPgUp", "Numpad_9"} },
        { VK_NUMPAD0,        0x52,           {"Num0","Numpad0", "NumIns", "Numpad_0"} },
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
        { VK_OEM_4,          0x1A,           {"[","{", "LeftBracket"} },
        { VK_OEM_6,          0x1B,           {"]","}", "RightBracket"} },
        { VK_OEM_5,          0x2B,           {"\\","|","BackSlash"} },
        //{ '[',               0x1A },
        //{ ']',               0x1B },
        //{ '\\',              0x2B },
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
        //{ '{',               0x1A | SHIFT_KEY },
        //{ '}',               0x1B | SHIFT_KEY },
        //{ '|',               0x2B | SHIFT_KEY },
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
        { ';',               0x27,           {";", "SemiColon"} },
        { VK_OEM_7,          0x28,           {"\'","\"","Apostrophe"} },
        //{ '\'',              0x28 },
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
        //{ '\"',              0x28 | SHIFT_KEY },
        { VK_RETURN,         0x1C,            { "Enter", "Return", "\n" } },
        { VK_SHIFT,          0x2A,            { "ShiftLeft", "Shift", "LeftShift" } },
        { 'z',               0x2C },
        { 'x',               0x2D },
        { 'c',               0x2E },
        { 'v',               0x2F },
        { 'b',               0x30 },
        { 'n',               0x31 },
        { 'm',               0x32 },
        { ',',               0x33,             {",", "Comma"} },
        { '.',               0x34,             {".", "Period"}  },
        { '/',               0x35,             {"/", "Slash"} },
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
        { VK_SHIFT,          0x36,            { "ShiftRight", "Shift", "RightShift" } },
        { VK_CONTROL,        0x1D,            { "CtrlLeft", "Ctrl", "LeftControl" } },
        { VK_LWIN,           0x5B | EXT_KEY,  { "LWin", "WinLeft", "Win", "Meta" } },
        { VK_MENU,           0x38,            { "AltLeft", "Alt", "LeftAlt" } },
        { VK_SPACE,          0x39,            { "Space", " "} },
        { VK_MENU,           0x38 | EXT_KEY,  { "AltRight", "Alt", "RightAlt"} },
        { VK_RWIN,           0x5C | EXT_KEY,  { "RWin", "WinRight", "Win", "Meta" } },
        { VK_APPS,           0x5D | EXT_KEY,  { "Apps", "ContextMenu", "Context"} },
        { VK_CONTROL,        0x1D | EXT_KEY,  { "CtrlRight", "Ctrl", "RightControl" } },
        { VK_UP,             0x48 | EXT_KEY,  { "Up", "UpArrow" } },
        { VK_LEFT,           0x4B | EXT_KEY,  { "Left", "LeftArrow" } },
        { VK_DOWN,           0x50 | EXT_KEY,  { "Down", "DownArrow" } },
        { VK_RIGHT,          0x4D | EXT_KEY,  { "Right", "RightArrow" }},
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
    //unsigned up_scancode = MapVirtualKeyA(VK_UP,    MAPVK_VK_TO_VSC_EX);
    //unsigned left_scancode = MapVirtualKey(VK_LEFT,  MAPVK_VK_TO_VSC_EX);
    //unsigned down_scancode = MapVirtualKey(VK_DOWN,  MAPVK_VK_TO_VSC_EX);
    //unsigned right_scancode = MapVirtualKey(VK_RIGHT, MAPVK_VK_TO_VSC_EX);
    for (const Key& key :  US_QWERTY_KEYBOARD_TABLE) {
        US_QWERTY_MAPPING_SC_TO_KEY.try_emplace(key.scanCode, key);
        if (key.vkCode)
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

static JOYSTICK_POSITION vJoyPosition;

static void addInterceptKey(const std::string name) {
    auto it = US_QWERTY_MAPPING_NAME_TO_KEY.find(name);
    if (it == US_QWERTY_MAPPING_NAME_TO_KEY.end()) {
        LOG(ERROR) << "Key '" << name << "' (requested to be intercepted) not found";
        return;
    }
    const Key& k = it->second;
    INTERCEPT_VK_KEY_SET.try_emplace(k.vkCode, k);
}

const std::vector<std::string>& getNamesForKey(const std::string& key) {
    auto it = US_QWERTY_MAPPING_NAME_TO_KEY.find(toLower(key));
    if (it != US_QWERTY_MAPPING_NAME_TO_KEY.end())
        return it->second.names;
    static std::vector<std::string> unknown {"unknown"};
    return unknown;
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

static bool addAxis(UINT devID, UINT axisID, const char* name, bool full) {
    if (!GetVJDAxisExist(devID, axisID))
        return false;
    LONG min, max;
    if (!GetVJDAxisMin(devID, axisID, &min))
        return false;
    if (!GetVJDAxisMax(devID, axisID, &max))
        return false;
    vJoyAxisMap[name] = {name, devID, axisID, min, max, full};
    return true;
}

bool acquire_vJoy() {
    unsigned rID = Cfg.getVJoyDeviceID();
    LOG(INFO) << "Initializing vJoy (device " << rID << ")";
    if (!vJoyEnabled()) {
        LOG(ERROR) << "Function vJoyEnabled Failed - make sure that vJoy is installed and enabled";
        return false;
    } else {
        std::wstring vendor = static_cast<TCHAR*> (GetvJoyManufacturerString());
        std::wstring product = static_cast<TCHAR*> (GetvJoyProductString());
        std::wstring version = static_cast<TCHAR*> (GetvJoySerialNumberString());
        LOG(INFO) << "vJoy Vendor : " << vendor;
        LOG(INFO) << "vJoy Product: " << product;
        LOG(INFO) << "vJoy Version: " << version;
    };
    WORD VerDll, VerDrv;
    if (!DriverMatch(&VerDll, &VerDrv))
        LOG(WARNING) << std::format("vJoy Driver (version {:04x}) does not match vJoyInterface DLL (version {:04x})", VerDrv,VerDll);
    else
        LOG(INFO) << std::format("vJoy Driver and vJoyInterface DLL match vJoyInterface DLL (version {:04x})", VerDrv);

    if (!isVJDExists(rID)) {
        LOG(ERROR) << "vJoy device " << rID << " not exists";
        return false;
    }
    VjdStat status = GetVJDStatus(rID);
    switch (status) {
    case VJD_STAT_OWN:
        LOG(INFO) << "vJoy device " << rID << " is already owned by EDRobot";
        break;
    case VJD_STAT_FREE:
        LOG(INFO) << "vJoy device " << rID << " is free";
        break;
    case VJD_STAT_BUSY:
        LOG(ERROR) << "vJoy device " << rID << " is already owned by another program";
        return false;
    case VJD_STAT_MISS:
        LOG(ERROR) << "vJoy device " << rID << " is not installed or disabled";
        return false;
    default:
        LOG(ERROR) << "vJoy device " << rID << " general error";
        return false;
    }
    // Acquire the vJoy device
    if (!AcquireVJD(rID)) {
        LOG(ERROR) << "Failed to acquire vJoy device " << rID;
        return false;
    } else {
        LOG(INFO) << "vJoy device " << rID << " acquired";
    }

    vJoyAcquired = true;
    ResetVJD(rID);
    vJoyAxisMap.clear();
    addAxis(rID, HID_USAGE_X, "Joy_XAxis", true);
    addAxis(rID, HID_USAGE_Y, "Joy_YAxis", true);
    addAxis(rID, HID_USAGE_Z, "Joy_ZAxis", true);

    reset_vJoy();

    return true;
}

bool reset_vJoy() {
    unsigned rID = Cfg.getVJoyDeviceID();
    //LOG(INFO) << "Reset vJoy (device " << rID << ")";
    ResetVJD(rID);
    for (auto it : vJoyAxisMap) {
        auto& ax = it.second;
        LONG reset = ax.min;
        if (ax.full)
            reset = std::round(double(ax.min+ax.max)*0.5);
        SetAxis(reset, ax.devID, ax.axisID);
    }
    return true;
}

bool release_vJoy() {
    reset_vJoy();
    unsigned rID = Cfg.getVJoyDeviceID();
    LOG(INFO) << "Release vJoy (device " << rID << ")";
    RelinquishVJD(rID);
    vJoyAcquired = false;
    return true;
}

void start(KeyboardCollbackFn callback) {
    keyboardShutdown = false;
    hKeyboardEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    keyboardCallback = callback;
    interceptor_thread = std::thread(&loop);
}

void stop() {
    keyboardShutdown = true;
    SetEvent(hKeyboardEvent);
    if (interceptor_thread.joinable()) {
        if (nativeThreadId) {
            PostThreadMessage(nativeThreadId, WM_QUIT, 0, 0);
            nativeThreadId = 0;
            if (hKeyboardEvent) {
                CloseHandle(hKeyboardEvent);
                hKeyboardEvent = nullptr;
            }
        }
        interceptor_thread.join();
    }
    if (hKeyboardEvent) {
        CloseHandle(hKeyboardEvent);
        hKeyboardEvent = nullptr;
    }
}

int getScanCode(std::string key_name) {
    std::wstring wkey = toUtf16(key_name);
    if (wkey[0] > 0x7f) {
        static std::vector<HKL> keyboardLayouts;
        if (keyboardLayouts.empty()) {
            int numLayouts = GetKeyboardLayoutList(0, nullptr);
            keyboardLayouts.resize(numLayouts);
            GetKeyboardLayoutList(numLayouts, keyboardLayouts.data());
        }
        short mapped_vk = -1;
        for (auto hkl : keyboardLayouts) {
            auto vk = VkKeyScanEx(wkey[0], hkl);
            if (vk < 0)
                continue;
            if (mapped_vk < 0) {
                mapped_vk = vk;
                continue;
            }
            if (mapped_vk >= 0 && mapped_vk == vk)
                continue;
            LOG(ERROR) << std::format("Umbigous key mapping for Key_{}", key_name);
        }
        if (mapped_vk >= 0) {
            char k = mapped_vk & 0x7F;
            key_name = std::string(&k, 1);
        }
    }

    auto it = US_QWERTY_MAPPING_NAME_TO_KEY.find(toLower(key_name));
    if (it == US_QWERTY_MAPPING_NAME_TO_KEY.end()) {
        LOG(ERROR) << "Scancode for " << key_name << " not found";
        return 0;
    }
    const Key& key = it->second;
    return key.scanCode;
}

static INPUT fillInput(const GameKey& gk, bool up) {
    INPUT input {};
    if (gk.device == GameKey::Keyboard) {
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = gk.code & 0xFF;
        input.ki.dwFlags = KEYEVENTF_SCANCODE;
        if (up)
            input.ki.dwFlags |= KEYEVENTF_KEYUP;
        if (gk.code & EXT_KEY)
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    else if (gk.device == GameKey::Mouse) {
        input.type = INPUT_MOUSE;
        if (gk.code == 1)
            input.mi.dwFlags |= up ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN;
        else if (gk.code == 2)
            input.mi.dwFlags |= up ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN;
        else if (gk.code == 3)
            input.mi.dwFlags |= up ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN;
    }
    return input;
}


void kbd_sleep(int milliseconds, bool precise) {
    ai::check_interrupted();
    if (milliseconds <= 0)
        return;
    if (milliseconds >= 75 && !precise) {
        auto now = std::chrono::high_resolution_clock::now();
        auto until = now + std::chrono::milliseconds(milliseconds);
        while (now < until) {
            auto left = std::chrono::duration_cast<std::chrono::milliseconds>(until - now);
            if (left.count() < 5)
                break;
            auto duration = std::min(std::chrono::milliseconds(500), left);
            std::this_thread::sleep_for(duration);
            now = std::chrono::high_resolution_clock::now();
        }
        ai::check_interrupted();
        return;
    }

    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    double seconds = milliseconds * 0.001;
    while (true) {
        QueryPerformanceCounter(&end);
        double elapsed_seconds = double(end.QuadPart - start.QuadPart) / double(frequency.QuadPart);
        if (elapsed_seconds >= seconds)
            break;
        ai::check_interrupted();
    }
}

bool send(const std::string& name, int delay_ms, int pause_ms, bool precise) {
    if (!Mgr.setGameForeground())
        return false;
    if (delay_ms <= 0)
        delay_ms = Cfg.getDefaultKeyHoldTime();
    if (pause_ms <= 0)
        pause_ms = Cfg.getDefaultKeyAfterTime();
    //LOG(INFO) << "send('" << name << "'," << delay_ms << "," << pause_ms << ")";
    const KeyBindings& keyBindings = Cfg.getGameKeyBindings(name);
    if (vJoyAcquired && keyBindings.secondary.device == GameKey::vJoy) {
        if (!post(keyBindings.secondary, delay_ms))
            return false;
        kbd_sleep(delay_ms + pause_ms, precise);
        return true;
    }
    else if (keyBindings.primary.device != GameKey::Void) {
         if (!post(keyBindings.primary, delay_ms))
            return false;
        kbd_sleep(delay_ms + pause_ms, precise);
        return true;
    }
    else if (keyBindings.secondary.device != GameKey::Void) {
        if (!post(keyBindings.secondary, delay_ms))
            return false;
        kbd_sleep(delay_ms + pause_ms, precise);
        return true;
    }
    int code = getScanCode(name);
    if (!code)
        return false;
    GameKey tmp {GameKey::Keyboard, name, code};
    if (!post(tmp, delay_ms))
        return false;
    kbd_sleep(delay_ms + pause_ms, precise);
    return true;
}

bool sendMouseMove(const cv::Point& point, int pause_ms, bool absolute) {
    if (!Mgr.setGameForeground())
        return false;
    bool virtualDesktop = false;
    int x = point.x;
    int y = point.y;
    if (absolute) {
        virtualDesktop = (GetSystemMetrics(SM_CMONITORS) > 1);
        cv::Point screen = Mgr.cvtReferenceToDesktop(point);
        x = screen.x;
        y = screen.y;
    }
    //LOG(INFO) << "sendMouseMove recalculated from reference " << point << " to screen " << screen;
    if (!sendMouseMoveTo(x, y, absolute, virtualDesktop))
        return false;
    kbd_sleep(pause_ms > 0 ? pause_ms : Cfg.getDefaultKeyAfterTime(), false);
    return true;
}

bool sendMouseDown(int buttons);
bool sendMouseUp(int buttons);
bool sendMouseClick(const cv::Point& point, int delay_ms, int pause_ms) {
    bool double_click = false;
    if (!Mgr.isGameForeground()) {
        if (!Mgr.setGameForeground())
            return false;
        double_click = true;
    }
    cv::Point screen = Mgr.cvtReferenceToDesktop(point);
    bool virtualDesktop = (GetSystemMetrics(SM_CMONITORS) > 1);
    //LOG(INFO) << "sendMouseClick recalculated from reference " << point << " to screen " << screen;
    if (!sendMouseMoveTo(screen.x, screen.y, true, virtualDesktop))
        return false;
    GameKey tmp {GameKey::Mouse, "Mouse_1", 1};
    if (double_click) {
        post(tmp, 50);
        kbd_sleep(100, false);
    }
    sendMouseDown(1);
    kbd_sleep(delay_ms, false);
    sendMouseUp(1);
    kbd_sleep(pause_ms, false);
    //if (!post(tmp, delay_ms))
    //    return false;
    //kbd_sleep(delay_ms + pause_ms, false);
    return true;
}

bool sendMouseMoveTo(int x, int y, bool absolute, bool virtualDesk) {
    DWORD flags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
    if (absolute) {
        flags |= MOUSEEVENTF_ABSOLUTE;
        if (virtualDesk) {
            // see https://stackoverflow.com/questions/62759122/calculate-normalized-coordinates-for-sendinput-in-a-multi-monitor-environment
            flags |= MOUSEEVENTF_VIRTUALDESK;
            x -= GetSystemMetrics(SM_XVIRTUALSCREEN);
            y -= GetSystemMetrics(SM_YVIRTUALSCREEN);
            x = MulDiv(x, 65536, GetSystemMetrics(SM_CXVIRTUALSCREEN));
            y = MulDiv(y, 65536, GetSystemMetrics(SM_CYVIRTUALSCREEN));
        } else {
            x = MulDiv(x, 65536, GetSystemMetrics(SM_CXSCREEN));
            y = MulDiv(y, 65536, GetSystemMetrics(SM_CYSCREEN));
        }
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

cv::Point getMouseDesktopPos() {
    POINT pnt {};
    GetCursorPos(&pnt);
    return cv::Point(pnt.x, pnt.y);
}

// value -1..1 for full-range axes, or 0..1 for others
bool axis(const KeyBindings& bindings, double value, bool background) {
    if (!(bindings.mode == KeyBindings::Axis || bindings.mode == KeyBindings::AxisInv))
        return false;
    if (!background && !Mgr.setGameForeground())
        return false;
    const GameKey& gk = bindings.primary;
    if (gk.device != GameKey::vJoy)
        return false;
    if (!vJoyAxisMap.contains(gk.key))
        return false;
    const vJoyAxisInfo& ax = vJoyAxisMap.at(gk.key);
    LONG val;
    if (ax.full) {
        value = std::clamp(value, -1.0, +1.0);
        if (bindings.mode == KeyBindings::AxisInv)
            value = -value;
        val = std::round(std::lerp(double(ax.min), double(ax.max), (value+1.0)*0.5));
    } else {
        value = std::clamp(value, 0.0, +1.0);
        val = std::round(std::lerp(double(ax.min), double(ax.max), value));
    }
    return SetAxis(val, ax.devID, ax.axisID);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYUP)) {
        auto* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        auto vkCode = pKeyBoard->vkCode;
        auto isInjected = pKeyBoard->flags & LLKHF_INJECTED;
        if (!isInjected && INTERCEPT_VK_KEY_SET.contains(vkCode)) {
            //LOG(INFO) << "keyboard hook key down: code " << pKeyBoard->vkCode << " scancode " << pKeyBoard->scanCode << " flags " << pKeyBoard->flags;
            int flags = 0;
            if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) flags |= LSHIFT;
            if (GetAsyncKeyState(VK_RSHIFT) & 0x8000) flags |= RSHIFT;
            if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) flags |= LCTRL;
            if (GetAsyncKeyState(VK_RCONTROL) & 0x8000) flags |= RCTRL;
            if (GetAsyncKeyState(VK_LMENU) & 0x8000) flags |= LALT;
            if (GetAsyncKeyState(VK_RMENU) & 0x8000) flags |= RALT;
            if (GetAsyncKeyState(VK_LWIN) & 0x8000) flags |= LWIN;
            if (GetAsyncKeyState(VK_RWIN) & 0x8000) flags |= RWIN;
            std::string keyName;
            const auto& it = US_QWERTY_MAPPING_VK_TO_NAME.find(vkCode);
            if (it != US_QWERTY_MAPPING_VK_TO_NAME.end())
                keyName = it->second.names[0];
            else
                keyName = unknownKeyName;
            keyboardCallback(pKeyBoard->vkCode, pKeyBoard->scanCode, flags, keyName);
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}


struct InputKey {
    GameKey::Device device;
    int code; // keyboards scancode, mouse or joystick button
    int counter;
    std::string_view name;
};
struct InputWait {
    unsigned inputId;
    std::chrono::time_point<std::chrono::high_resolution_clock> end;
    std::string name;
    InputKey* keys[8]; // first is a primary key index, others are modifiers
};

unsigned inputKeysSize = 0;
static InputKey inputKeys[64];
static std::vector<InputWait> inputWait;

//static void logInputKeys() {
//    {
//        std::string msg;
//        for (auto &iw: inputWait) {
//            if (!msg.empty())
//                msg += ", ";
//            msg += iw.name;
//            msg += std::format("({})", iw.inputId);
//        }
//        LOG(INFO) << "inputWait: " << msg;
//    }
//    {
//        std::string msg;
//        for (auto &ik: inputKeys) {
//            if (!msg.empty())
//                msg += ", ";
//            msg += ik.name;
//            msg += std::format("({})", ik.counter);
//        }
//        LOG(INFO) << "inputKeys: " << msg;
//    }
//}

//static void logKeyboardState() {
//    std::string msg;
//
//    if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
//        msg += "LShift ";
//    if (GetAsyncKeyState(VK_RSHIFT) & 0x8000)
//        msg += "RShift ";
//    if (GetAsyncKeyState(VK_LCONTROL) & 0x8000)
//        msg += "LCtrl ";
//    if (GetAsyncKeyState(VK_RCONTROL) & 0x8000)
//        msg += "RCtrl ";
//    if (GetAsyncKeyState(VK_LMENU) & 0x8000)
//        msg += "LAlt ";
//    if (GetAsyncKeyState(VK_RMENU) & 0x8000)
//        msg += "RAlt ";
//
//    for (auto& ik : inputKeys) {
//        if (ik.device == GameKey::Device::Void || ik.counter <= 0)
//            continue;
//        if (ik.device == GameKey::Device::Mouse) {
//            msg += std::format("btn {} ", ik.code);
//            continue;
//        }
//        if (ik.device == GameKey::Device::vJoy) {
//            msg += std::format("joy {} ", ik.code);
//            continue;
//        }
//        if (ik.device == GameKey::Device::Keyboard) {
//            msg += std::format("{}/{} ", ik.name, ik.counter);
//            continue;
//        }
//    }
//
//    if (!msg.empty())
//        LOG(INFO) << "keyboard: " << msg;
//}

InputKey* addInputKey(const GameKey& gk) {
    if (gk.device == GameKey::Void)
        return 0;
    for (unsigned i=0; i < inputKeysSize; i++) {
        auto& ik = inputKeys[i];
        if (gk.device == ik.device && gk.code == ik.code) {
            ik.counter += 1;
            return &ik;
        }
    }
    for (unsigned i=0; i < inputKeysSize; i++) {
        auto& ik = inputKeys[i];
        if (ik.device == GameKey::Device::Void && ik.counter == 0) {
            ik.device = gk.device;
            ik.code = gk.code;
            ik.counter = 1;
            ik.name = gk.key;
            return &ik;
        }
    }
    auto& ik = inputKeys[inputKeysSize];
    ik.device = gk.device;
    ik.code = gk.code;
    ik.counter = 1;
    ik.name = gk.key;
    inputKeysSize += 1;
    return &ik;
}

unsigned addInputWait(const GameKey& gk, int hold) {
    InputWait& wait = inputWait.emplace_back();
    wait.name = gk.key;
    wait.inputId = keyboardInputCounter.fetch_add(1);
    if (!wait.inputId)
        wait.inputId = keyboardInputCounter.fetch_add(1);
    wait.end = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(hold);
    int i = 0;
    wait.keys[i++] = addInputKey(gk);
    for (auto& km : gk.modifiers) {
        wait.name = km.key + '+' + wait.name;
        wait.keys[i++] = addInputKey(km);
    }
    //LOG(INFO) << "addInputWait ("<<wait.inputId<<") " << wait.name << " hold " << hold;
    //logInputKeys();
    return wait.inputId;
}

int64_t getNextWakeupTime() {
    std::unique_lock<std::mutex> lock(keyboardMutex);
    std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> next = now + std::chrono::milliseconds(5000);
    InputWait* nw = nullptr;
    for (auto& iw : inputWait) {
        if (iw.end < next) {
            next = iw.end;
            nw = &iw;
        }
    }
    auto dur= next - now;
    int milli = std::chrono::duration_cast<std::chrono::duration<int64_t,std::milli>>(dur).count();
    //LOG(INFO) << "getNextWakeupTime ("<<(nw?nw->inputId:0)<<") wait " << milli << " ms";
    return milli;
}


void releaseKeys() {
    for (unsigned i=0; i < inputKeysSize; i++) {
        auto& ik = inputKeys[i];
        if (ik.counter > 0 || ik.device == GameKey::Void)
            continue;
        //LOG(INFO) << "releaseKey " << ik.name;
        if (ik.device == GameKey::Keyboard) {
            INPUT input {};
            input.type = INPUT_KEYBOARD;
            input.ki.wScan = ik.code & 0xFF;
            input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
            if (ik.code & EXT_KEY)
                input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
            unsigned sent = SendInput(1, &input, sizeof(INPUT));
            if (sent != 1)
                LOG(ERROR) << "SendInput failed: " << getErrorMessage();
        }
        else if (ik.device == GameKey::Mouse && ik.code >= 1 && ik.code <= 3) {
            INPUT input {};
            input.type = INPUT_MOUSE;
            switch (ik.code) {
            case 1: input.mi.dwFlags |= MOUSEEVENTF_LEFTUP; break;
            case 2: input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP; break;
            case 3: input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP; break;
            }
            unsigned sent = SendInput(1, &input, sizeof(INPUT));
            if (sent != 1)
                LOG(ERROR) << "SendInput failed: " << getErrorMessage();
        }
        else if (ik.device == GameKey::vJoy && ik.code > 0) {
            SetBtn(FALSE, Cfg.getVJoyDeviceID(), ik.code);
        }
        ik.device = GameKey::Void;
    }
    for (int i=inputKeysSize-1; i >= 0; i--) {
        auto& ik = inputKeys[i];
        if (ik.device != GameKey::Void || ik.counter > 0)
            break;
        inputKeysSize = i;
    }
    //logInputKeys();
}

void processKeyRelease() {
    std::unique_lock<std::mutex> lock(keyboardMutex);
    std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> exp = now - std::chrono::milliseconds(3);
    for (auto it=inputWait.begin(); it < inputWait.end(); ) {
        auto& iw = *it;
        if (iw.end > exp) {
            it++;
            continue;
        }
        for (auto ik : iw.keys) {
            if (ik) {
                ik->counter -= 1;
                ik = 0;
            }
        }
        //LOG(INFO) << "processKeyRelease id:" << iw.inputId << " erase " << iw.name;
        it = inputWait.erase(it);
    }
    releaseKeys();
}

unsigned post(const std::string& name, int hold_ms) {
    if (!Mgr.setGameForeground())
        return 0;
    //LOG(INFO) << "holdKeyDown('" << name << ")";
    const KeyBindings& keyBindings = Cfg.getGameKeyBindings(name);
    if (keyBindings.primary.device != GameKey::Void) {
        return post(keyBindings.primary, hold_ms);
    }
    else if (keyBindings.secondary.device != GameKey::Void) {
        return post(keyBindings.secondary, hold_ms);
    }
    int code = getScanCode(name);
    if (!code)
        return false;
    GameKey tmp {GameKey::Keyboard, name, code};
    return post(tmp, hold_ms);
}

unsigned post(const GameKey& gk, int hold_ms) {
    std::unique_lock<std::mutex> lock(keyboardMutex);
    unsigned inputId = addInputWait(gk, hold_ms + 10*gk.modifiers.size());
    for (auto& gkm : gk.modifiers) {
        if (gkm.device == GameKey::Void)
            continue;
        else if (gkm.device == GameKey::vJoy && gkm.code > 0)
            SetBtn(TRUE, Cfg.getVJoyDeviceID(), gkm.code);
        else if (gkm.device == GameKey::Keyboard) {
            INPUT input {};
            input.type = INPUT_KEYBOARD;
            input.ki.wScan = gkm.code & 0xFF;
            input.ki.dwFlags = KEYEVENTF_SCANCODE;
            if (gkm.code & EXT_KEY)
                input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
            unsigned sent = SendInput(1, &input, sizeof(INPUT));
            if (sent != 1)
                LOG(ERROR) << "SendInput keydown '" << gkm << "' failed: " << getErrorMessage();
            kbd_sleep(10, true);
        }
        else if (gkm.device == GameKey::Mouse) {
            INPUT input {};
            input.type = INPUT_MOUSE;
            if (gkm.code == 1) input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
            else if (gkm.code == 2) input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
            else if (gkm.code == 3) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
            unsigned sent = SendInput(1, &input, sizeof(INPUT));
            if (sent != 1)
                LOG(ERROR) << "SendInput mouse button '" << gkm << "' failed: " << getErrorMessage();
            kbd_sleep(10, true);
        }
    }
    if (gk.device == GameKey::Keyboard) {
        INPUT input {};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = gk.code & 0xFF;
        input.ki.dwFlags = KEYEVENTF_SCANCODE;
        if (gk.code & EXT_KEY)
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        unsigned sent = SendInput(1, &input, sizeof(INPUT));
        if (sent != 1)
            LOG(ERROR) << "SendInput keydown '" << gk << "' failed: " << getErrorMessage();
    }
    if (gk.device == GameKey::Mouse) {
        INPUT input {};
        input.type = INPUT_MOUSE;
        if (gk.code == 1) input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
        else if (gk.code == 2) input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
        else if (gk.code == 3) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
        unsigned sent = SendInput(1, &input, sizeof(INPUT));
        if (sent != 1)
            LOG(ERROR) << "SendInput mouse button '" << gk << "' failed: " << getErrorMessage();
    }
    if (gk.device == GameKey::vJoy && gk.code > 0)
        SetBtn(TRUE, Cfg.getVJoyDeviceID(), gk.code);
    return inputId;
}

bool clearInput(unsigned inputId) {
    bool ok = false;
    {
        std::unique_lock<std::mutex> lock(keyboardMutex);
        std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
        for (auto &iw: inputWait) {
            if (iw.inputId == inputId) {
                iw.end = now;
                ok = true;
                //LOG(INFO) << "clearInput " << inputId;
            }
        }
    }
    processKeyRelease();
    return ok;
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
    SetThreadDescription(GetCurrentThread(), L"Keyboard interceptor");

    const int NumHandles = 1;
    HANDLE handles[NumHandles] = {hKeyboardEvent};
    for (;;) {
        if (keyboardShutdown)
            break;
        int64_t waitMillis = getNextWakeupTime();
        if (waitMillis <= 0) {
            processKeyRelease();
            continue;
        }
        //logKeyboardState();
        ResetEvent(hKeyboardEvent);
        DWORD dwResult = MsgWaitForMultipleObjects(NumHandles, handles, FALSE, waitMillis, QS_ALLINPUT);
        if (keyboardShutdown)
            break;
        if (dwResult == WAIT_OBJECT_0+NumHandles) {
            // A message is available, retrieve it with GetMessage or PeekMessage
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    UnhookWindowsHookEx(keyboardHook);
    std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
    for (auto& iw : inputWait) {
        iw.end = now;
    }
    processKeyRelease();
    inputWait.clear();
    for (auto& ik : inputKeys)
        ik.counter = 0;
    releaseKeys();
    //logInputKeys();

    nativeThreadId = 0;
}

} // namespace keyboard
