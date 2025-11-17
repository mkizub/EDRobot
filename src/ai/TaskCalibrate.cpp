//
// Created by mkizub on 02.07.2025.
//

#include "../pch.h"

#include "TaskCalibrate.h"
#include "AIManager.h"

#include "../detect/Detector.h"
#include "../EDWidget.h"
#include "../Keyboard.h"

namespace ai {

TaskCalibrate::TaskCalibrate(const TaskTemplate& templ_)
        : Task(templ_)
{
    assert(templ.id == ED_TASK_CALIBRATE);
}

void TaskCalibrate::recordButton(const char* button, WState bs) {
    cv::Rect rect = Mgr.resolveWidgetReferenceRect(button);
    if (rect.empty()) {
        LOG(ERROR) << "Cannot get rect of button '" << button << "'";
        return;
    }
    ClassifyEnv cEnv;
    cEnv.init(ai::rEnv, &colorImage, nullptr);
    detect::Histogram histDet(detect::Histogram::Mode::BGR, rect);
    histDet.calc(cEnv);
    cv::Vec3b bgr = histDet.mLastColor;
    mButtonBGR[int(bs)].push_back(bgr);
    const char* names[] = {"Normal   ", "Focused  ", "Active   ", "Disabled "};
    LOG(INFO) << names[int(bs)] << " button: bgr=" << mButtonBGR[int(bs)].back()
              << " rgb=0x"<< std::format("{:06x}", decodeBGR(bgr));
}

void TaskCalibrate::recordLstRow(const char* list, cv::Point mouse, WState bs) {
    cv::Rect rect = Mgr.resolveWidgetReferenceRect(list);
    if (rect.empty()) {
        LOG(ERROR) << "Cannot get rect of list '" << list << "'";
        return;
    }
    std::vector<cv::Vec3b> colors;
    std::vector<double> lums;
    for (auto& cr : ai::rEnv.classified) {
        if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != list)
            continue;
        ClassifyEnv cEnv;
        cEnv.init(ai::rEnv, &colorImage, nullptr);
        cv::Rect refRect = cr.detectedRect;
        detect::Histogram histDet(detect::Histogram::Mode::BGR, rect);
        histDet.calc(cEnv);
        if (refRect.contains(mouse)) {
            mLstRowBGR[int(WState::Focused)].push_back(histDet.mLastColor);
        } else {
            colors.push_back(histDet.mLastColor);
            lums.push_back(sBgr2Hsv(colors.back())[2]);
        }
    }
    cv::Point minLoc;
    cv::Point maxLoc;
    cv::minMaxLoc(lums, nullptr, nullptr, &minLoc, &maxLoc);
    cv::Vec3d darkColor(colors[minLoc.x]);
    cv::Vec3d lightColor(colors[maxLoc.x]);
    double lumDelta = lums[maxLoc.x] - lums[minLoc.x];

    if (lumDelta < 6) {
        mLstRowBGR[int(bs)].insert(mLstRowBGR[int(bs)].end(), colors.begin(), colors.end());
    } else {
        for (auto& c : colors) {
            double distNorm = distanceBGR(darkColor, c);
            double distActv = distanceBGR(lightColor, c);
            if (distNorm < distActv)
                mLstRowBGR[int(WState::Normal)].push_back(c);
            else
                mLstRowBGR[int(WState::Active)].push_back(c);
        }
    }
}

bool TaskCalibrate::calculateAverage(bool incomplete) {
    bool buttonSuccess = true;
    for(auto ws : enum_values<WState>()) {
        if (ws == WState::Unknown)
            continue;
        auto& bgrState = mButtonBGR[int(ws)];
        int len = (int)bgrState.size();
        if (!len) {
            LOG(INFO) << "No samples for " << enum_name(ws) << " button color";
            if (!incomplete)
                return false;
            continue;
        }
        cv::Mat colorsMatrix(len, 1, CV_8UC3);
        for (int j=0; j < len; j++)
            colorsMatrix.at<cv::Vec3b>(j) = bgrState[j];
        cv::Scalar meanS;
        cv::Scalar stddevS;
        cv::meanStdDev(colorsMatrix, meanS, stddevS);
        cv::Vec3b mean(meanS[0], meanS[1], meanS[2]);
        cv::Vec3d stddev(stddevS[0], stddevS[1], stddevS[2]);
        LOG(INFO) << "BGR button color for " << enum_name(ws) << " mean " << mean << " stddev " << stddev << " over " << len << " samples";
        mButtonBGRAverage[int(ws)] = mean;
        if (stddevS[0] > 3 || stddevS[1] > 3 || stddevS[2] > 3) {
            buttonSuccess = false;
            LOG(ERROR) << "Luv color for " << enum_name(ws) << ", has too high deviation " << stddev;
        }
    }
    bool lstRowSuccess = true;
    for(auto ws : enum_values<WState>()) {
        if (ws == WState::Unknown)
            continue;
        auto& bgrState = mLstRowBGR[int(ws)];
        int len = (int)bgrState.size();
        if (!len) {
            LOG(INFO) << "No samples for " << enum_name(ws) << " list row color";
            continue;
        }
        cv::Mat colorsMatrix(len, 1, CV_8UC3);
        for (int j=0; j < len; j++)
            colorsMatrix.at<cv::Vec3b>(j) = bgrState[j];
        cv::Scalar meanS;
        cv::Scalar stddevS;
        cv::meanStdDev(colorsMatrix, meanS, stddevS);
        cv::Vec3b mean(meanS[0], meanS[1], meanS[2]);
        cv::Vec3d stddev(stddevS[0], stddevS[1], stddevS[2]);
        LOG(INFO) << "Luv list row color for " << enum_name(ws) << " mean " << mean << " stddev " << stddev << " over " << len << " samples";
        mLstRowBGRAverage[int(ws)] = mean;
        if (stddevS[0] > 3 || stddevS[1] > 3 || stddevS[2] > 3) {
            lstRowSuccess = false;
            LOG(ERROR) << "Luv color for " << enum_name(ws) << ", has too high deviation " << stddev;
        }
    }
    Cfg.setCalibrationResult(mButtonBGRAverage, mLstRowBGRAverage);
    return buttonSuccess;
}

void TaskCalibrate::getRowsByState(const ClassifiedRect** rows) {
    for (int i=0; i < 4; i++)
        rows[i] = nullptr;
    for (auto &row: ai::rEnv.classified) {
        if (row.cdt != ClsDetType::ListRow || row.u.lrow.list->name != "lst-goods")
            continue;
        WState ws = row.u.lrow.ws;
        if (ws == WState::Unknown)
            continue;
        if (rows[int(ws)] == nullptr)
            rows[int(ws)] = &row;
    }
}

bool TaskCalibrate::run() {
    ai::detectEDState(DetectLevel::Buttons, &colorImage, nullptr);
    if (!ai::uiState.match("scr-market:*"))
        throw_failed("Not at market, calibration fails");

    notify_progress(MSG_INFO, "Calibration started");

    //
    // Detect normal, focused, activated colors using buttons
    //

    if (ai::uiState.match("scr-market:mod-sell")) {
        hardcodedStep("{click:'btn-to-sell', after: 500}", DetectLevel::Buttons, &colorImage, nullptr);
    }
    else if (ai::uiState.match("scr-market:mod-buy")) {
        hardcodedStep("{click:'btn-to-buy', after: 500}", DetectLevel::Buttons, &colorImage, nullptr);
    }

    hardcodedStep("{goto:'btn-exit', after: 500}", DetectLevel::Buttons, &colorImage, nullptr);
    LOG(INFO) << "State " << ai::uiState << " expected focused 'btn-exit'";

    recordButton("btn-help", WState::Normal);
    recordButton("btn-exit", WState::Focused);
    recordButton("btn-filter", WState::Normal);
    if (ai::uiState.match("scr-market:mod-sell")) {
        recordButton("btn-to-sell", WState::Active);
        recordButton("btn-to-buy", WState::Normal);
    }
    else if (ai::uiState.match("scr-market:mod-buy")) {
        recordButton("btn-to-sell", WState::Normal);
        recordButton("btn-to-buy", WState::Active);
    }

    hardcodedStep("{goto:'btn-help', after: 500}", DetectLevel::Screen, &colorImage, nullptr);
    LOG(INFO) << "State " << ai::uiState << " expected focused 'btn-help'";

    recordButton("btn-help", WState::Focused);
    recordButton("btn-exit", WState::Normal);
    recordButton("btn-filter", WState::Normal);
    if (ai::uiState.match("scr-market:mod-sell")) {
        recordButton("btn-to-sell", WState::Active);
        recordButton("btn-to-buy", WState::Normal);
    }
    else if (ai::uiState.match("scr-market:mod-buy")) {
        recordButton("btn-to-sell", WState::Normal);
        recordButton("btn-to-buy", WState::Active);
    }

    hardcodedStep("{goto:'btn-filter', after: 500}", DetectLevel::Buttons, &colorImage, nullptr);
    LOG(INFO) << "State " << ai::uiState << " expected focused 'btn-filter'";

    recordButton("btn-help", WState::Normal);
    recordButton("btn-exit", WState::Normal);
    recordButton("btn-filter", WState::Focused);
    if (ai::uiState.match("scr-market:mod-sell")) {
        recordButton("btn-to-sell", WState::Active);
        recordButton("btn-to-buy", WState::Normal);
    }
    else if (ai::uiState.match("scr-market:mod-buy")) {
        recordButton("btn-to-sell", WState::Normal);
        recordButton("btn-to-buy", WState::Active);
    }

    hardcodedStep("{goto:'btn-to-buy', after: 500}", DetectLevel::Buttons, &colorImage, nullptr);
    LOG(INFO) << "State " << ai::uiState << " expected focused 'btn-to-buy'";

    recordButton("btn-help", WState::Normal);
    recordButton("btn-exit", WState::Normal);
    recordButton("btn-filter", WState::Normal);
    recordButton("btn-to-buy", WState::Focused);

    hardcodedStep("{goto:'btn-to-sell', after: 500}", DetectLevel::Buttons, &colorImage, nullptr);
    LOG(INFO) << "State " << ai::uiState << " expected focused 'btn-to-sell'";

    recordButton("btn-help", WState::Normal);
    recordButton("btn-exit", WState::Normal);
    recordButton("btn-filter", WState::Normal);
    recordButton("btn-to-sell", WState::Focused);

    calculateAverage(true);

    //
    // Goto sell market
    //

    hardcodedStep("{click:'btn-to-sell', after: 1000}", DetectLevel::ListRows, &colorImage, nullptr);
    LOG(INFO) << "State " << ai::uiState << " expected state 'scr-market:mod-sell'";
    if (!ai::uiState.match("scr-market:mod-sell"))
        throw_failed("Not at market sell, calibration fails");

    //
    // Detect normal list rows in sell market
    //
    {
        for (auto &cr: ai::rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                continue;
            cv::Point mouse = (cr.detectedRect.tl() + cr.detectedRect.br()) / 2;
            kbd::sendMouseMove(mouse, 300);
            ai::detectEDState(DetectLevel::ListRows, &colorImage, nullptr);
            recordLstRow("lst-goods", mouse, WState::Normal);
            if (mLstRowBGR[int(WState::Normal)].size() > 35)
                break;
        }
    }

    calculateAverage(true);
    ai::detectEDState(DetectLevel::ListRows, &colorImage, nullptr);

    const ClassifiedRect* list_rows[4];
    getRowsByState(list_rows);
    const ClassifiedRect* row_to_test = list_rows[int(WState::Normal)];
    if (!row_to_test)
        throw_failed("Cannot find commodity to test sell dialog, calibration fails");

    //
    // found commodity to test, check we detected list row correctly
    //

    {
        auto& row_rect = row_to_test->detectedRect;
        cv::Point row_point = (row_rect.tl() + row_rect.br()) / 2;
        std::ostringstream goto_str;
        goto_str << "{goto:" << row_point << ", after:500}";
        hardcodedStep(goto_str.str().c_str(), DetectLevel::ListRows, &colorImage, nullptr);
        LOG(INFO) << "State " << ai::uiState;
    }
    ai::detectEDState(DetectLevel::ListRows, &colorImage, nullptr);
    getRowsByState(list_rows);
    row_to_test = list_rows[int(WState::Focused)];
    if (!row_to_test)
        throw_failed("Cannot find commodity to test sell dialog, calibration fails");

    hardcodedStep("[{key:'UI_Select', after:2000},"
                  "{check:'scr-market:mod-sell:dlg-trade:*'},"
                  "{goto:'btn-more', after:500}]", DetectLevel::Buttons, &colorImage, nullptr);
    LOG(INFO) << "State " << ai::uiState;

    recordButton("btn-cancel", WState::Normal);
    recordButton("btn-more", WState::Focused);
    recordButton("btn-commit", WState::Disabled);

    hardcodedStep("[{key:'UI_Left'},"
                  "{key:'UI_Select', after:1000},"
                  "{check:'scr-market:mod-sell'},"
                  "{click:'btn-to-buy', after:1000},"
                  "{check:'scr-market:mod-buy'},"
                  "{goto:'btn-help', after:500}]", DetectLevel::ListRows, &colorImage, nullptr);
    LOG(INFO) << "State " << ai::uiState << " expected focused 'btn-help'";

    recordButton("btn-exit", WState::Normal);
    recordButton("btn-help", WState::Focused);
    recordButton("btn-to-buy", WState::Active);

    //
    // Detect activated list rows in sell market
    //
    {
        for (auto &cr: ai::rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                continue;
            cv::Point mouse = (cr.detectedRect.tl() + cr.detectedRect.br()) / 2;
            kbd::sendMouseMove(mouse, 300);
            ai::detectEDState(DetectLevel::ListRows, &colorImage, nullptr);
            recordLstRow("lst-goods", mouse, WState::Active);
            if (mLstRowBGR[int(WState::Active)].size() > 35)
                break;
        }
    }

    calculateAverage(true);
    ai::detectEDState(DetectLevel::ListRows, &colorImage, nullptr);

    getRowsByState(list_rows);
    row_to_test = list_rows[int(WState::Active)];
    if (!row_to_test)
        throw_failed("Cannot find commodity to test buy dialog, calibration fails");

    {
        auto& row_rect = row_to_test->detectedRect;
        cv::Point row_point = (row_rect.tl() + row_rect.br()) / 2;
        std::ostringstream goto_str;
        goto_str << "{goto:" << row_point << ", after:500}";
        hardcodedStep(goto_str.str().c_str(), DetectLevel::ListRows, &colorImage, nullptr);
        LOG(INFO) << "State " << ai::uiState;
    }

    hardcodedStep("[{key:'UI_Select', after:2000},"
                  "{check:'scr-market:mod-buy:dlg-trade:*'},"
                  "{goto:'btn-more', after:500}]", DetectLevel::Buttons, &colorImage, nullptr);
    LOG(INFO) << "State " << ai::uiState;

    recordButton("btn-cancel", WState::Normal);
    recordButton("btn-more", WState::Focused);
    recordButton("btn-commit", WState::Disabled);

    hardcodedStep("[{key:'UI_Left'},"
                  "{key:'UI_Select', after:500},"
                  "{check:'scr-market:mod-buy'},"
                  "{click:'btn-to-sell', after:500},"
                  "{check:'scr-market:mod-sell'},"
                  "{goto:'btn-exit', after:500}]", DetectLevel::Buttons, &colorImage, nullptr);

    LOG(INFO) << "State " << ai::uiState;

    if (calculateAverage(false)) {
        notify_progress(MSG_INFO, "Calibration completed successfully!");
        Cfg.saveCalibration();
    } else {
        throw_failed("Failed to calibrate button state detector");
    }
    return true;
}


}
