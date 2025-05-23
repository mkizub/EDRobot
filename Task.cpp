//
// Created by mkizub on 23.05.2025.
//

#include "Task.h"
#include "Template.h"
#include "Keyboard.h"
#include "easylogging++.h"
#include <winuser.h>
#include <synchapi.h>
#include <sysinfoapi.h>

static void high_precision_sleep_qpc(double seconds) {
    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    while (true) {
        QueryPerformanceCounter(&end);
        double elapsed_seconds = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
        if (elapsed_seconds >= seconds) {
            break;
        }
    }
}

void Task::sleep(int milliseconds) {
    check_completed();
    if (milliseconds <= 0)
        return;

    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    double seconds = milliseconds * 0.001;
    while (true) {
        QueryPerformanceCounter(&end);
        double elapsed_seconds = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
        if (elapsed_seconds >= seconds)
            break;
        check_completed();
    }
}

void Task::sendKey(const char* name, int delay_ms, int pause_ms) {
    try {
        keyboard::sendDown(name);
        if (delay_ms > 0)
            sleep(delay_ms);
        keyboard::sendUp(name);
        if (pause_ms > 0)
            sleep(pause_ms);
    } catch (...) {
        keyboard::sendUp(name);
        throw;
    }
}

bool TaskSell::run() {
    if (done) {
        LOG(INFO) << "done at start";
        return false;
    }
    try {
        while (mSells > 0) {
            EDState edState = master.getEDState();
            if (edState == EDState::MarketSell) {
                LOG(INFO) << "At market, press 'space' to start trading dialog";
                sendKey("space", 35, 300);
                continue;
            }
            if (edState == EDState::MarketSellDialog) {
                LOG(INFO) << "At market sell dialog";
                sendKey("up");
                sendKey("up");
                sendKey("up");
                sendKey("left", 2300);
                for (int i=0; i < mItems; i++)
                    sendKey("right");
                sendKey("down", 35, 100);
                sendKey("space", 35, 1500);
                mSells -= 1;
                continue;
            }
            LOG(INFO) << "Unknown state " << int(edState);
            return false;
        }
    } catch (completed_error& e) {
        return false;
    }
    done = true;
    return true;
}
