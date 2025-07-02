//
// Created by mkizub on 02.07.2025.
//

#include "../pch.h"

#include "TaskDebug.h"
#include "AIManager.h"
#include "../EDWidget.h"

#include <tesseract/baseapi.h>

namespace ai {

TaskDebugFindAllCommodities::TaskDebugFindAllCommodities(Task* parent, AIManager& mgr, const TaskTemplate& templ)
        : Task(parent, mgr, templ)
        , shuffle(false)
        , dump_index(4)
        , start_index(0)
{
    assert (templ.name == ED_TASK_DEBUG_FILE_ALL_COMMODITIES);
}

Result TaskDebugFindAllCommodities::run() {
    mgr.detectEDState(DetectLevel::Screen);
    if (!mgr.uiState.match("scr-market:*"))
        notifyError("Not in market?", Result::Failure);
    std::string marketMode = mgr.uiState.path().substr(11);
    std::vector<Commodity*> table;
    if (marketMode == "mod-sell")
        table = mgr.cfg.getMarketInSellOrder();
    else if (marketMode == "mod-buy")
        table = mgr.cfg.getMarketInBuyOrder();
    else
        notifyError("Unknown market mode "+marketMode, Result::Failure);
    if (table.empty())
        notifyError("Empty market?", Result::Failure);
    struct VerifyStats {
        int ocr_min_conf = 100;
        int ocr_max_conf = 0;
        int fuzzy_min_conf = 100;
        int fuzzy_max_conf = 0;
        int total_samples = 0;
    };
    std::map<const Commodity*,VerifyStats> verifyMap;
    int verifyUnrecognized = 0;

    std::deque<Commodity *> checkCommoditiesTable(table.begin(), table.end());
    int passed = 0;
    int failed = 0;
    if (shuffle) {
        std::srand(std::time({}));
    } else {
        for (int i=0; i < start_index; i++)
            checkCommoditiesTable.pop_front();
    }
    int left = checkCommoditiesTable.size();
    int checkIdx = 0;
    while (!checkCommoditiesTable.empty()) {
        if (shuffle)
            checkIdx = std::rand() % checkCommoditiesTable.size();
        Commodity *commodity = checkCommoditiesTable[checkIdx];
        std::vector<CommodityMatch> verify;
        bool ok = checkCommodity(commodity, marketMode, table, &verify);
        if (!ok)
            failed += 1;
        else
            passed += 1;
        left -= 1;
        notifyProgress("Test for commodity '"+commodity->name+"' "+(ok?" PASSED\n":" FAILED\n")+
                       "Progress: "+std::to_string(passed)+" passed and "+std::to_string(failed)+" failed\n"+
                       "left "+std::to_string(left)+" out of "+std::to_string(table.size()-start_index));
        std::erase(checkCommoditiesTable, commodity);
        for (auto& v : verify) {
            if (!v.commodity) {
                verifyUnrecognized += 1;
                continue;
            }
            auto& vs = verifyMap[v.commodity];
            if (v.ocr_conf < vs.ocr_min_conf)
                vs.ocr_min_conf = v.ocr_conf;
            if (v.ocr_conf > vs.ocr_max_conf)
                vs.ocr_max_conf = v.ocr_conf;
            if (v.fuzzy_conf < vs.fuzzy_min_conf)
                vs.fuzzy_min_conf = v.fuzzy_conf;
            if (v.fuzzy_conf > vs.fuzzy_max_conf)
                vs.fuzzy_max_conf = v.fuzzy_conf;
            vs.total_samples += 1;
        }
    }

    LOG(INFO) << "OCR/Fuzzy match statistic:";
    for (auto c : table) {
        auto& vs = verifyMap[c];
        LOG(INFO) << "  '" << c->name << "': ocr=" << vs.ocr_min_conf << ".." << vs.ocr_min_conf
                  << "; fuzzy=" << vs.fuzzy_min_conf << ".." << vs.fuzzy_max_conf;
    }
    LOG(INFO) << "  totally unrecognized: " << verifyUnrecognized;

    Sleep(1000);
    if (!checkCommoditiesTable.empty())
        notifyProgress("Cannot verify all commodities");
    notifyProgress("All commodities verified");
    return Result::Success;
}

bool TaskDebugFindAllCommodities::checkCommodity(Commodity *currCommodity, const std::string &marketMode,
                                                 const std::vector<Commodity *> &table,
                                                 std::vector<CommodityMatch> *verify) {
    for (;;) {
        cv::Mat grayImage;
        mgr.detectEDState(DetectLevel::ListOcrFocusedRow, nullptr, &grayImage);
        if (!mgr.uiState.match("scr-market:"+marketMode)) {
            notifyProgress("Not at market?");
            return false;
        }
        if (!mgr.master.approximateListOfCommodities(mgr.rEnv, grayImage, "lst-goods", table, verify)) {
            notifyProgress("Cannot detect commodities in 'lst-goods', aborting");
            return false;
        }
        mgr.rEnv.classified = mgr.rEnv.classified;
        const ClassifiedRect* focusedRow = nullptr;
        const Commodity* focusedCommodity = nullptr;
        for (auto &cr: mgr.rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                continue;
            const Commodity* rowCommodity = cr.u.lrow.commodity;
            if (!rowCommodity)
                rowCommodity = mgr.cfg.getCommodityByName(cr.text, true);
            if (cr.u.lrow.ws == WState::Focused) {
                focusedRow = &cr;
                LOG(INFO) << "Focused row text: " << focusedRow->text;
                focusedCommodity = rowCommodity;
                if (focusedCommodity)
                    LOG(INFO) << "Focused commodity: " << focusedCommodity->name;
            }
            if (focusedCommodity == currCommodity) {
                LOG(INFO) << "Row with required commodity '" << focusedCommodity->name << "' found, focused";
                break;
            }
            if (rowCommodity == currCommodity) {
                LOG(INFO) << "Row with required commodity '" << rowCommodity->name << "' found, not focused";
                cv::Point mouse = (cr.detectedRect.tl() + cr.detectedRect.br()) / 2;
                sendMouseMove(mouse, 100);
                focusedCommodity = currCommodity;
                focusedRow = &cr;
                break;
            }
        }
        if (!focusedRow) {
            LOG(INFO) << "No focused row found, moving mouse to the list area";
            cv::Rect rect = mgr.master.resolveWidgetReferenceRect("lst-goods");
            int x = rect.x+rect.width/2;
            int y = rect.y - 20;
            sendMouseClick({x,y}, 0, 500);
            for (int i=0; i < 10; i++)
                sendMouseMove({0, 10}, 25, false);
            continue;
        }
        if (!focusedCommodity) {
            notifyProgress("Cannot detect commodities in 'lst-goods', aborting");
            return false;
        }
        if (focusedCommodity == currCommodity) {
            /*if (dlgCommodity == currCommodity)*/ {
                mgr.detectEDState(DetectLevel::ListOcrFocusedRow, nullptr, &grayImage);
                cv::Rect r = mgr.rEnv.cvtReferenceToCaptured(focusedRow->detectedRect);
                saveOcrTrainingData(grayImage, r, currCommodity, false);
            }
            hardcodedStep("[{key:'UI_Select', after:200},"
                          "{wait: 'scr-market:"+marketMode+":dlg-trade:*', during: 3000},"
                                                           "{goto:'btn-cancel', after:200}]",
                          DetectLevel::Buttons);
            const Commodity* dlgCommodity = mgr.master.getLabelCommodity("lbl-commodity");
            if (dlgCommodity != currCommodity) {
                notifyProgress("Dialog commodity mismatch");
                Sleep(3000);
            }
            hardcodedStep("[{ key: 'UI_Select' },"
                          "{ wait: 'scr-market:"+marketMode+"', during: 3000 }]",
                          DetectLevel::Buttons);
            return (dlgCommodity == currCommodity);
        }

        int focusedIdx = -1;
        int needIdx = -1;
        for (int idx = 0; idx < table.size(); idx++) {
            auto &c = table[idx];
            if (c == focusedCommodity)
                focusedIdx = idx;
            if (c == currCommodity)
                needIdx = idx;
        }
        if (needIdx >= 0 && focusedIdx >= 0) {
            LOG(INFO) << "Distance is "<<(needIdx - focusedIdx)<<" lines from focused '" << table[focusedIdx]->name << "' to '" << currCommodity->name << "'";
            if (needIdx < focusedIdx) {
                int count = focusedIdx - needIdx;
                for (int cnt=0; cnt < count; cnt++)
                    sendKey("up");
                count = std::min(3, needIdx);
                for (int cnt=0; cnt < count; cnt++)
                    sendKey("up");
                for (int cnt=0; cnt < count; cnt++)
                    sendKey("down");
            } else {
                int count = needIdx-focusedIdx;
                for (int cnt=0; cnt < needIdx-focusedIdx; cnt++)
                    sendKey("down");
                count = std::min(shuffle ? 3 : 10, int(table.size())-1-needIdx);
                for (int cnt=0; cnt < count; cnt++)
                    sendKey("down");
                for (int cnt=0; cnt < count; cnt++)
                    sendKey("up");
            }
            continue;
        }
        notifyProgress("Cannot detect commodities in 'lst-goods', aborting");
        return false;
    }
}

void TaskDebugFindAllCommodities::saveOcrTrainingData(const cv::Mat& grayImage, cv::Rect rect, const Commodity* commodity, bool invert) {
    std::string lng;
    if (mgr.cfg.lng == Lang::RU)
        lng += "rus";
    else if (mgr.cfg.lng == Lang::EN)
        lng += "eng";
    else if (mgr.cfg.lng)
        lng += "xxx";
    std::string filename = std::format("testset-{}/{}-{:02d}-{}.png", lng, commodity->nameId, dump_index, lng);
    if (std::filesystem::exists(filename))
        return;

    tesseract::TessBaseAPI* tesseractApi = mgr.master.getTesseractApi();
    if (!tesseractApi)
        return;
    cv::Mat rowImage(grayImage, rect);
    int outConf = 0;
    std::string text;
    if (!invert) {
        tesseractApi->SetImage(rowImage.data, rowImage.cols, rowImage.rows, 1, rowImage.step);
        tesseractApi->Recognize(nullptr);
        tesseract::ResultIterator* ri = tesseractApi->GetIterator();
        const char* outText = ri->GetUTF8Text(tesseract::PageIteratorLevel::RIL_TEXTLINE);
        outConf = ri->Confidence(tesseract::PageIteratorLevel::RIL_TEXTLINE);
        int left, top, right, bottom;
        ri->BoundingBox(tesseract::PageIteratorLevel::RIL_TEXTLINE, 2, &left, &top, &right, &bottom);
        text = trim(outText);
        delete[] outText;
        cv::Rect textRect = {rect.tl()+cv::Point(left,top), rect.tl()+cv::Point(right,bottom)};
        LOG(INFO) << "OCR Output: '" << text << "' words conf=" << outConf << "%" << " rect: " << textRect;
        if (textRect.empty() || outConf == 0)
            textRect = rect;
        {
            cv::Mat textImage(grayImage, textRect);
            cv::imwrite(filename, textImage);
            filename = std::format("testset-{}/{}-{:02d}-{}.gt.txt", lng, commodity->nameId, dump_index, lng);
            std::ofstream gt_txt(filename, std::ios::trunc | std::ios::binary);
            gt_txt << commodity->name;
            gt_txt.close();
            return;
        }
    }

//    if (invert) {
//        // try hard - detect background, and if it's dark - threshold and invert the image
//        int histSize = 256;
//        float range[]{0, 256}; //the upper boundary is exclusive
//        const float *histRange[]{range};
//        cv::Mat hist;
//        cv::calcHist(&rowImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
//        int maxLoc[4]{};
//        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
//        int background = maxLoc[0] + 5;
//        if (background > 127)
//            return 0;
//
//        cv::Mat invertedImage;
//        cv::bitwise_not(rowImage, invertedImage);
//        cv::Mat thrImage;
//        cv::threshold(invertedImage, thrImage, 255 - background, 255, cv::THRESH_BINARY);
//        mTesseractApiForMarket->SetImage(thrImage.data, thrImage.cols, thrImage.rows, 1, thrImage.step);
//        char *outText = mTesseractApiForMarket->GetUTF8Text();
//        text = trim(outText);
//        outConf = mTesseractApiForMarket->MeanTextConf();
//        delete[] outText;
//        if (outConf > 30) {
//            LOG(INFO) << "OCR Output: '" << text << "' words conf=" << outConf << "% (retried with negative)";
//            return outConf;
//        }
//    }
//    return outConf;
}

} // ai