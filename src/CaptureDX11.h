//
// Created by mkizub on 13.06.2025.
//

#pragma once

#include "pch.h"


#ifndef EDROBOT_CAPTUREDXGI_H
#define EDROBOT_CAPTUREDXGI_H


class CaptureDX11 {
public:
    static CaptureDX11* getEDCapturer(HWND hWndED);

    ~CaptureDX11();
    virtual bool captureWindow() = 0;
    virtual bool playWindow() = 0;

protected:
    CaptureDX11(HWND hWndED) : hWndED(hWndED) {}

    HWND hWndED;
};


#endif //EDROBOT_CAPTUREDXGI_H
