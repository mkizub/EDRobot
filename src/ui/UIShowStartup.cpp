//
// Created by mkizub on 22.03.2026.
//

#include "../pch.h"

#include "UIShowStartup.h"
#include "UILayout.h"

const int ctrlIdBase      = 0x8100;
const int IDC_MESSAGES    = ctrlIdBase + 1;
const int IDC_VERSION     = ctrlIdBase + 2;

UIShowStartup::UIShowStartup() : UIControl(false)
{
}

UIShowStartup::UIShowStartup(const std::string &message, std::string latest_version, std::string latest_url)
        : UIControl(false)
        , startup_message(message)
        , latest_version(latest_version)
        , latest_url(latest_url)
{
    on_message(WM_CTLCOLORSTATIC, [this](wl::params p) -> INT_PTR {
        if ((HWND)p.lParam == lbl_version.hwnd()) {
            HDC hdcStatic = (HDC)p.wParam;
            // Set the text color to blue (RGB(0, 0, 255))
            SetTextColor(hdcStatic, RGB(0, 0, 0xEE));
            // Optional: Set the background mode to transparent if the parent window has a custom background
            SetBkMode(hdcStatic, TRANSPARENT);
            // Return a handle to the brush you want for the background
            // We use the system's default window background color brush here
            return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
        }
        return 0;
    });
    this->base_msg_pubm::on_command(IDC_VERSION, [this](wl::params p){
        if (HIWORD(p.wParam) == STN_CLICKED && !this->latest_url.empty()) {
            auto url = toUtf16(this->latest_url);
            // Use ShellExecute to open the URL in the default browser.
            // The "open" verb is used to perform the default operation on the specified file or URL.
            // SW_SHOWNORMAL or SW_SHOW specifies how the window is to be shown.
            ShellExecuteW(
                    NULL,           // hWnd: Parent window handle (NULL for no parent)
                    L"open",        // lpOperation: Operation to perform ("open", "edit", "print", etc.)
                    url.c_str(),    // lpFile: The file, folder, or URL to operate on
                    NULL,           // lpParameters: Parameters for the executable (not needed for URLs)
                    NULL,           // lpDirectory: Default directory (NULL for current directory)
                    SW_SHOWNORMAL   // nShowCmd: How to show the launched application's window
            );
        }
        return 0;
    });
}

UIShowStartup::~UIShowStartup() {
}

void UIShowStartup::initialize() {
    lbl_message.create(hwnd(), IDC_MESSAGES, L"", {0,0}, {404,421})
            .style.set_style(true, WS_BORDER | SS_CENTER);

    lbl_version.create(hwnd(), IDC_VERSION, L"", {0,411}, {404,20})
            .style.set_style(true, WS_BORDER | SS_CENTER | SS_NOTIFY)
            .style.set_style_ex(true, WS_EX_TRANSPARENT);

    lbl_message.set_text(toUtf16("\n\n\n"+startup_message));

    std::string version;
    if (latest_version == EDROBOT_VERSION)
        version = lc_format("Version: {}", EDROBOT_VERSION);
    else
        version = lc_format("Version: {}, available {}", EDROBOT_VERSION, latest_version);

    lbl_version.set_text(toUtf16(version));
}

void UIShowStartup::relayout(bool scroll_to_top) {
    if (scroll_to_top)
        reset_scroll(true);
    RECT rect{};
    GetClientRect(hwnd(), &rect);

    int uiPercent = Cfg.getUiScalePercents();
    int uiDpi = GetDpiForWindow(hwnd());
    UILayout lo(uiDpi, uiPercent, rect);
    if (uiDpi != scaled_to_dpi) {
        scaled_to_dpi = uiDpi;
        loCreateFont(font, uiDpi, uiPercent);
        font.set_on(lbl_message);
        font.set_on(lbl_version);
    }

    lo.wpi = BeginDeferWindowPos(10);

    int w = lo.width - lo.left;
    int h = lo.height - lo.top - lo.vrow - lo.vgap;
    lo.wpi = DeferWindowPos(lo.wpi, lbl_message.hwnd(), nullptr, lo.left, lo.top, w, h, SWP_NOZORDER);
    lo.top += h + lo.vgap;
    lo.wpi = DeferWindowPos(lo.wpi, lbl_version.hwnd(), nullptr, lo.left, lo.top, w, lo.vrow, SWP_NOZORDER);
    lo.top += lo.vrow;

    EndDeferWindowPos(lo.wpi);
}

