//
// Created by mkizub on 11.03.2026.
//

#include "../pch.h"

#include "UIShowTask.h"
#include "UILayout.h"
#include "../ai/AIManager.h"

const int ctrlIdBase      = 0x8100;
const int IDC_TASK_STATUS = ctrlIdBase + 1;
const int IDC_STATUS      = ctrlIdBase + 2;

UIShowTask::UIShowTask() : UIControl(false)
{
}

UIShowTask::UIShowTask(const std::string &message, std::string latest_version, std::string latest_url)
    : UIControl(false)
    , startup_message(message)
    , latest_version(latest_version)
    , latest_url(latest_url)
{
}

UIShowTask::~UIShowTask() {
}

void UIShowTask::initialize() {
    lbl_task_status.create(hwnd(), IDC_TASK_STATUS, L"", {0,0}, {404,421})
            .style.set_style(true, WS_BORDER);

    lbl_status.create(hwnd(), IDC_STATUS, L"", {0,411}, {404,20})
            .style.set_style(true, WS_BORDER | WS_EX_TRANSPARENT);

    on_timer_update();
}

void UIShowTask::relayout() {
    RECT rect{};
    GetClientRect(hwnd(), &rect);

    int uiPercent = Cfg.getUiScalePercents();
    int uiDpi = GetDpiForWindow(hwnd());
    UILayout lo(uiDpi, uiPercent, rect);
    if (uiDpi != scaled_to_dpi) {
        scaled_to_dpi = uiDpi;
        loCreateFont(font, uiDpi, uiPercent);
        font.set_on(lbl_task_status);
        font.set_on(lbl_status);
    }

    lo.wpi = BeginDeferWindowPos(10);

    int w = lo.width - lo.left;
    int h = lo.height - lo.top - lo.vrow - lo.vgap;
    lo.wpi = DeferWindowPos(lo.wpi, lbl_task_status.hwnd(), nullptr, lo.left, lo.top, w, h, SWP_NOZORDER);
    lo.top += h + lo.vgap;
    lo.wpi = DeferWindowPos(lo.wpi, lbl_status.hwnd(), nullptr, lo.left, lo.top, w, lo.vrow, SWP_NOZORDER);
    lo.top += lo.vrow;

    EndDeferWindowPos(lo.wpi);
}

void UIShowTask::on_timer_update() {
    if (!ai::curr_task() && !ai::last_task() && !startup_message.empty()) {
        if (startup_shown)
            return;
        lbl_task_status.set_text(toUtf16("\n\n\n"+startup_message));
        lbl_task_status.style.set_style(true, SS_CENTER);

        std::string version;
        if (latest_version == EDROBOT_VERSION)
            version = lc_format("Version: {}", EDROBOT_VERSION);
        else
            version = lc_format("Version: {}, available {}", EDROBOT_VERSION, latest_version);

        lbl_status.set_text(toUtf16(version));
        lbl_status.style.set_style(true, SS_CENTER);
        startup_shown = true;
        return;
    }
    else if (!startup_message.empty()) {
        startup_message.clear();
        lbl_task_status.style.set_style(false, SS_CENTER);
        lbl_status.style.set_style(false, SS_CENTER);
    }

    bool completed = false;
    bool failed = false;
    ai::spTask task = ai::curr_task();
    if (!task) {
        task = ai::last_task();
        completed = true;
        failed = task && task->progress == ai::TaskExitReason::FAILED;
    }
    std::string status;
    int indent = 0;
    for (ai::spStep step=task; step; step = step->currSubStep) {
        status += std::string(indent, ' ');
        status += step->getTitle();
        status += ":\n";
        indent += 4;
        if (step->prevSubStep) {
            status += std::string(indent, ' ');
            status += step->prevSubStep->getTitle();
            status += "\n";
            for (auto& msg : step->prevSubStep->getMessages()) {
                if (msg.empty())
                    continue;
                status += std::string(indent+4, ' ');
                status += msg;
                status += "\n";
            }
        }
        if (!step->currSubStep || failed) {
            for (auto& msg : step->getMessages()) {
                if (msg.empty())
                    continue;
                status += std::string(indent, ' ');
                status += msg;
                status += "\n";
            }
            for (auto& msg : split(step->getStatus(), '\n')) {
                if (msg.empty())
                    continue;
                status += std::string(indent, ' ');
                status += msg;
                status += "\n";
            }
        }
        if (failed)
            break;
    }
    lbl_task_status.set_text(toUtf16(status));

    if (!task) {
        lbl_status.set_text(L"");
    }
    else if (completed) {
        if (task->progress == ai::TaskExitReason::FAILED)
            lbl_status.set_text(toUtf16(_gt("Finished (failed)")).c_str());
        else
            lbl_status.set_text(toUtf16(_gt("Finished")).c_str());
    }
    else if (!ai::active()) {
        lbl_status.set_text(toUtf16(_gt("Paused (inactive)")).c_str());
    }
    else if (ai::isDebugPause()) {
        lbl_status.set_text(toUtf16(_gt("Paused (active)")).c_str());
    }
    else if (st::ship.flags.docked) {
        lbl_status.set_text(toUtf16(_gt("Docked")).c_str());
    }
    else if (st::ship.flags.landed) {
        lbl_status.set_text(toUtf16(_gt("Landed")).c_str());
    }
    else if (st::ship.flags.fsd_jump) {
        lbl_status.set_text(toUtf16(_gt("Hyperspace")).c_str());
    }
    else {
        std::string space = st::ship.flags.cruise ? _gt("Cruise") : _gt("Space");
        std::string spd = "??";
        if (st::autopilot.speed_set_to.has_value())
            spd = std::to_string(st::autopilot.speed_set_to.value());
        std::string dist = "??";
        if (st::autopilot.isDestBodyTargeted && st::autopilot.distanceToBody)
            dist = st::autopilot.distanceToBody.to_string();
        if (st::autopilot.isDestDockTargeted && st::autopilot.distanceToDock)
            dist = st::autopilot.distanceToDock.to_string();
        lbl_status.set_text(toUtf16(lc_format("{}: speed {}%, distance {}", space, spd, dist)));
    }
}
