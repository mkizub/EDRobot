//
// Created by mkizub on 02.07.2025.
//

#include "../pch.h"

#include "TaskDebug.h"
#include "AIManager.h"
#include "../Keyboard.h"
#include "../widget/EDWidget.h"
#include "../widget/List.h"
#include "../FuzzyMatch.h"
#include "../OCR.h"
#include "../Galaxy.h"

#include <tesseract/baseapi.h>
#include <curl/curl.h>

namespace ai {

void TaskDebugFindAllBase::checkAndFixOCRText() {
    std::string dirname = "testset-edr";

    LOG(INFO) << "Checking OCR texts in dir: " << dirname;
    FuzzyMatch fuzzyMatch;
    int errorCount = 0;
    for (const auto &entry: std::filesystem::directory_iterator(dirname)) {
        if (!entry.is_regular_file())
            continue;
        auto &ep = entry.path();
        if (!ep.has_extension() || ep.extension() != ".txt")
            continue;

        std::vector<std::wstring> fixedLines;
        {
            std::ifstream ifs(ep, std::ifstream::in);
            if (!ifs.is_open()) {
                LOG(ERROR) << "Cannot open text file: " << ep << " for reading";
                continue;
            }
            bool badChar = false;
            std::string line;
            while ( getline(ifs, line) ) {
                std::wstring lineWide = toUtf16(line);
                std::wstring lineOCR = fuzzyMatch.toOCR(lineWide);
                if (lineOCR != lineWide) {
                    LOG(ERROR) << "Bad OCR char in text file: " << ep << " text: " << toUtf8(lineWide);
                    badChar = true;
                }
                fixedLines.push_back(lineOCR);
            }
            ifs.close();
            if (!badChar)
                continue;
        }
        errorCount += 1;
        {
            std::ofstream ofs(ep, std::ifstream::out | std::ifstream::trunc);
            if (!ofs.is_open()) {
                LOG(ERROR) << "Cannot open text file: " << ep << " for writing";
                continue;
            }
            bool first = true;
            for (auto& line : fixedLines) {
                if (!first)
                    ofs << '\n';
                first = false;
                ofs << toUtf8(line);
            }
            ofs.close();
        }
    }
    if (!errorCount)
        LOG(INFO) << "No OCR text error found";
    else
        LOG(INFO) << "Fixed " << errorCount << " OCR text errors";
}

TaskDebugFindAllCommodities::TaskDebugFindAllCommodities(const TaskTemplate& templ_)
        : TaskDebugFindAllBase(templ_, true)
        , shuffle(false)
        , dump_images(false)
        , start_index(0)
{
    assert (templ.id == ED_TASK_DEBUG_FIND_ALL_COMMODITIES);
    for (auto& p : templ.params) {
        if (p.id == "shuffle")
            shuffle = p.as_boolean();
        if (p.id == "dump_images")
            dump_images = p.as_boolean();
        if (p.id == "start_index")
            start_index = p.as_integer();
    }
}

bool TaskDebugFindAllCommodities::run() {
    checkAndFixOCRText();
    ai::detectEDState(DetectLevel::Screen);
    if (!ai::uiState.match("scr-market:*"))
        throw_failed("Not in market?");
    std::string marketMode = ai::uiState.path().substr(11);
    std::vector<Commodity*> table;
    if (marketMode == "mod-sell")
        table = Cfg.getMarketInSellOrder();
    else if (marketMode == "mod-buy")
        table = Cfg.getMarketInBuyOrder();
    else
        throw_failed("Unknown market mode {}", marketMode);
    if (table.empty())
        throw_failed("Empty market?");
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
    int offset = start_index;
    int checkIdx = 0;
    while (!checkCommoditiesTable.empty()) {
        if (shuffle) {
            checkIdx = std::rand() % checkCommoditiesTable.size();
        }
        Commodity *commodity = checkCommoditiesTable[checkIdx];
        if (!shuffle) {
            LOG(INFO) << "Testing offset " << offset << " out of " << table.size() << " commodity '"
                      << commodity->nameId << "' (" << commodity->name << ")";
        }
        std::vector<CommodityMatch> verify;
        bool ok;
        if (!dump_images)
            ok = checkCommodity(commodity, marketMode, table, &verify);
        else
            ok = checkCommodity(commodity, marketMode, table, nullptr);
        if (!ok)
            failed += 1;
        else
            passed += 1;
        left -= 1;
        offset += 1;
        notify_progress_(MSG_INFO, "Test for commodity '"+commodity->name+"' "+(ok?" PASSED\n":" FAILED\n")+
                       "Progress: "+std::to_string(passed)+" passed and "+std::to_string(failed)+" failed\n"+
                       "left "+std::to_string(left)+" out of "+std::to_string(table.size()-start_index));
        std::erase(checkCommoditiesTable, commodity);
        if (!dump_images) {
            for (auto &v: verify) {
                if (!v.commodity) {
                    verifyUnrecognized += 1;
                    continue;
                }
                auto &vs = verifyMap[v.commodity];
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
        notify_progress_(MSG_WARN, "Cannot verify all commodities");
    else
        notify_progress_(MSG_INFO, "All commodities verified");
    return true;
}

bool TaskDebugFindAllCommodities::checkCommodity(Commodity *currCommodity, const std::string &marketMode,
                                                 const std::vector<Commodity *> &table,
                                                 std::vector<CommodityMatch> *verify)
{
    if (dump_images) {
        std::string lbl_filename = std::format("testset-edr/{}-lbl-gray.png", currCommodity->nameId);
        std::string row_filename = std::format("testset-edr/{}-row-gray.png", currCommodity->nameId);
        if (std::filesystem::exists(lbl_filename) && std::filesystem::exists(row_filename)) {
            LOG(INFO) << "Files already generated for: " << currCommodity->nameId;
            return true;
        }
    }

    for (;;) {
        sleep(500);
        cv::Mat grayImage;
        ai::detectEDStateGrayIm(DetectLevel::ListRows, grayImage);
        if (!ai::uiState.match("scr-market:"+marketMode)) {
            if (ai::uiState.match("scr-market:"+marketMode+":dlg-trade:*")) {
                kbd::send("UI_Back", 50, 1000);
                continue;
            }
            notify_progress_(MSG_ERROR, "Not at market?");
            return false;
        }
        if (!Mgr.approximateListOfCommodities(ai::rEnv, grayImage, "lst-goods", table, verify)) {
            notify_progress_(MSG_ERROR, "Cannot detect commodities in 'lst-goods', aborting");
            return false;
        }
        const ClassifiedRect* focusedRow = nullptr;
        const Commodity* focusedCommodity = nullptr;
        bool isOnScreen = false;
        for (auto &cr: ai::rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.list->name != "lst-goods")
                continue;
            const Commodity* rowCommodity = cr.u.lrow.commodity;
            if (!rowCommodity)
                rowCommodity = Cfg.getCommodityByName(cr.text, true);
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
            if (rowCommodity == currCommodity)
                isOnScreen = true;
        }
        if (!focusedRow) {
            LOG(INFO) << "No focused row found, moving mouse to the list area";
            cv::Rect rect = Mgr.resolveWidgetReferenceRect("lst-goods", ai::rEnv);
            int x = rect.x+rect.width/2;
            int y = rect.y - 20;
            kbd::sendMouseClick({x, y}, 0, 500);
            for (int i=0; i < 10; i++)
                kbd::sendMouseMove({0, 10}, 25, false);
            continue;
        }
        if (!focusedCommodity) {
            notify_progress_(MSG_ERROR, "Cannot detect commodities in 'lst-goods', aborting");
            return false;
        }
        if (focusedCommodity == currCommodity) {
            saveOcrMarketRow(grayImage, *focusedRow, currCommodity);

            hardcodedStep("[{key:'UI_Select', after:200},"
                          "{wait: 'scr-market:"+marketMode+":dlg-trade:*', during: 3000},"
                          "{sleep: 1000}]",
                          DetectLevel::Buttons, &grayImage);
            const Commodity* dlgCommodity = Master::getLabelCommodity(ai::rEnv, grayImage, "lbl-commodity");
            if (dlgCommodity != currCommodity) {
                notify_progress_(MSG_WARN, "Dialog commodity mismatch");
                Sleep(1000);
            }
            {
                for (auto& cr : ai::rEnv.classified) {
                    if (cr.cdt == ClsDetType::Widget && cr.text == "lbl-commodity" && cr.u.widg.widget->tp == widget::WidgetType::Label) {
                        saveOcrMarketLbl(grayImage, cr, currCommodity);
                        break;
                    }
                }
            }
            hardcodedStep("[{ key: 'UI_Back' },"
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
                    kbd::send("UI_Up");
                if (!isOnScreen) {
                    count = std::min(3, needIdx);
                    for (int cnt = 0; cnt < count; cnt++)
                        kbd::send("up");
                    for (int cnt = 0; cnt < count; cnt++)
                        kbd::send("down");
                }
            } else {
                int count = needIdx-focusedIdx;
                for (int cnt=0; cnt < count; cnt++)
                    kbd::send("UI_Down");
                if (!isOnScreen) {
                    count = std::min(shuffle ? 3 : 10, int(table.size()) - 1 - needIdx);
                    for (int cnt = 0; cnt < count; cnt++)
                        kbd::send("down");
                    for (int cnt = 0; cnt < count; cnt++)
                        kbd::send("up");
                }
            }
            continue;
        }
        notify_progress_(MSG_ERROR, "Cannot detect commodities in 'lst-goods', aborting");
        return false;
    }
}

void TaskDebugFindAllCommodities::saveOcrMarketRow(const cv::Mat& grayImage, const ClassifiedRect& cr, const Commodity* commodity) {
    if (!dump_images)
        return;
    assert(cr.cdt == ClsDetType::ListRow && cr.u.lrow.ws == WState::Focused && cr.u.lrow.commodity);
    if (cr.cdt != ClsDetType::ListRow || cr.u.lrow.ws != WState::Focused || !cr.u.lrow.commodity)
        return;
    assert(cr.u.lrow.commodity == commodity);
    if (commodity != cr.u.lrow.commodity)
        return;
    std::string filename = std::format("testset-edr/{}-row-gray.png", commodity->nameId);
    if (std::filesystem::exists(filename))
        return;

    std::string text;
    cv::Mat rowDumpImage;
    int conf = ocr::ocrRowTextForTraining(ocr::GENERIC, grayImage, ai::rEnv, cr, "name", text, rowDumpImage);

    filename = std::format("testset-edr/{}-row-gray.png", commodity->nameId);
    cv::imwrite(filename, rowDumpImage);

//    cv::Mat rowOtsuImage;
//    cv::threshold(rowDumpImage, rowOtsuImage, 150, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
//    filename = std::format("testset-edr/{}-row-otsu.png", commodity->nameId);
//    cv::imwrite(filename, rowOtsuImage);

    FuzzyMatch fm;
    std::wstring nameOCR = fm.toOCR(commodity->wide);

    std::ofstream gt_txt;
    filename = std::format("testset-edr/{}-row-gray.gt.txt", commodity->nameId);
    gt_txt.open(filename, std::ios::trunc | std::ios::binary);
    gt_txt << toUtf8(nameOCR);
    gt_txt.close();
//    filename = std::format("testset-edr/{}-row-otsu.gt.txt", commodity->nameId);
//    gt_txt.open(filename, std::ios::trunc | std::ios::binary);
//    gt_txt << toUtf8(nameOCR);
//    gt_txt.close();
}

void TaskDebugFindAllCommodities::saveOcrMarketLbl(const cv::Mat& grayImage, const ClassifiedRect& cr, const Commodity* commodity) {
    if (!dump_images)
        return;
    assert(cr.cdt == ClsDetType::Widget && cr.u.widg.widget->tp == widget::WidgetType::Label);
    if (cr.cdt != ClsDetType::Widget || cr.u.widg.widget->tp != widget::WidgetType::Label)
        return;
    assert(commodity);
    if (!commodity)
        return;
    std::string filename = std::format("testset-edr/{}-lbl-gray.png", commodity->nameId);
    if (std::filesystem::exists(filename))
        return;

    std::vector<std::string> texts;
    std::vector<cv::Mat> lblDumpImages;
    int conf = ocr::ocrMarketLblTextForTraining(grayImage, ai::rEnv, cr, texts, lblDumpImages);

    assert (texts.size() == lblDumpImages.size());

    FuzzyMatch fm;
    bool textsMatch = true;
    std::vector<std::wstring> dumpTexts;
    if (texts.size() == 1) {
        dumpTexts.push_back(commodity->wocr);
    } else {
        std::string text;
        for (int l=0; l < texts.size(); l++) {
            dumpTexts.push_back(fm.toOCR(toUtf16(texts[l])));
            text += " " + texts[l];
        }
        std::wstring wocr = fm.toOCR(toUtf16(trim(text)));
        if (wocr != commodity->wocr)
            textsMatch = false;
    }

    for (int l=0; l < texts.size(); l++) {
        if (l > 0)
            filename = std::format("testset-edr/{}-lbl{}-gray.png", commodity->nameId, l);
        else
            filename = std::format("testset-edr/{}-lbl-gray.png", commodity->nameId);
        cv::imwrite(filename, lblDumpImages[l]);

//        cv::Mat lblOtsuImage;
//        cv::threshold(lblDumpImages[l], lblOtsuImage, 150, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
//        if (l > 0)
//            filename = std::format("testset-edr/{}-lbl{}-otsu.png", commodity->nameId, l);
//        else
//            filename = std::format("testset-edr/{}-lbl-otsu.png", commodity->nameId);
//        cv::imwrite(filename, lblOtsuImage);

        if (l > 0)
            filename = std::format("testset-edr/{}-lbl{}-gray.gt.txt", commodity->nameId, l);
        else
            filename = std::format("testset-edr/{}-lbl-gray.gt.txt", commodity->nameId);
        std::ofstream gt_txt(filename, std::ios::trunc | std::ios::binary);
        if (textsMatch) {
            gt_txt << toUtf8(dumpTexts[l]);
        } else {
            if (l == 0)
                gt_txt << toUtf8(commodity->wocr);
        }
        gt_txt.close();

//        if (l > 0)
//            filename = std::format("testset-edr/{}-lbl{}-otsu.gt.txt", commodity->nameId, l);
//        else
//            filename = std::format("testset-edr/{}-lbl-otsu.gt.txt", commodity->nameId);
//        gt_txt.open(filename, std::ios::trunc | std::ios::binary);
//        if (textsMatch) {
//            gt_txt << toUtf8(dumpTexts[l]);
//        } else {
//            if (l == 0)
//                gt_txt << toUtf8(commodity->wocr);
//        }
//        gt_txt.close();
    }
}

TaskDebugFindAllNavPoints::TaskDebugFindAllNavPoints(const TaskTemplate &templ)
    : TaskDebugFindAllBase(templ, false)
    , dump_images(false)
    , resume(false)
    , ocr_confidence(90)
    , txt_confidence(90)
    , offset_append(0)
{
    assert (templ.id == ED_TASK_DEBUG_FIND_ALL_NAV_POINTS);
    for (auto& p : templ.params) {
        if (p.id == "dump_images")
            dump_images = p.as_boolean();
        if (p.id == "resume")
            resume = p.as_boolean();
        if (p.id == "unfocused")
            unfocused = p.as_boolean();
        if (p.id == "ocr_confidence")
            ocr_confidence = p.as_integer();
        if (p.id == "txt_confidence")
            txt_confidence = p.as_integer();
    }
    //std::string filename = std::format("testset-edr/nav-{}-lbl-gray.png", lng, offset);
    //std::string filename = std::format("testset-edr/nav-{}-row-gray.png", lng, offset);
    //std::string filename = std::format("testset-edr/nav-{}-num-gray.png", lng, offset);
    std::string dirname = "testset-edr";

    int max_offset = -1;
    for (const auto &entry: std::filesystem::directory_iterator(dirname)) {
        if (!entry.is_regular_file())
            continue;
        auto &ep = entry.path();
        if (!ep.has_extension() || ep.extension() != ".png")
            continue;
        if (!ep.filename().string().starts_with("nav-"))
            continue;
        auto strings = split(ep.filename().string(), '-');
        int nav = std::stoi(strings[1], nullptr, 10);
        max_offset = std::max(nav, max_offset);
    }
    offset_append = max_offset+1;
    LOG(INFO) << "Start nav numbering offset: " << offset_append;
}

bool TaskDebugFindAllNavPoints::run() {
    checkAndFixOCRText();
    ai::detectEDState(DetectLevel::Screen);
    if (!ai::uiState.match("scr-left-panel:mod-nav-list"))
        throw_failed("Not in mod-nav-list?");

    if (!getSpanishInfo())
        LOG(ERROR) << "Cannot get system info from spansh.co.uk";

    if (!(resume || unfocused)) {
        kbd::send("UI_Right");
        kbd::send("UI_Down");
        kbd::send("UI_Up", 2000, 100);
    }

    int offset = 0;
    int failCount=0;
    if (unfocused) {
        cv::Mat grayImage;
        ai::detectEDStateGrayIm(DetectLevel::ListRows, grayImage);
        for (auto &cr: ai::rEnv.classified) {
            if (cr.cdt != ClsDetType::ListRow)
                continue;
            int conf = ocr::ocrRowText(ocr::GENERIC, grayImage, ai::rEnv, cr, "name", cr.text);
            cr.u.lrow.text_confidence = conf;
            if (conf <= ocr_confidence || checkOcrError(cr)) {
                LOG(INFO) << "Checking nav-point at offset " << offset << ", detected conf=" << conf << "%";
                saveOcrNavigationRow(grayImage, cr, offset, "", nullptr);
                offset += 1;
            }
        }
    } else {
        for (;;) {
            int row = 0;
            int focused_row = -1;
            std::vector<int> rows_with_error;
            ClassifiedRect *focused = nullptr;
            cv::Mat grayImage;
            ai::detectEDStateGrayIm(DetectLevel::ListRows, grayImage);
            for (auto &cr: ai::rEnv.classified) {
                if (cr.cdt != ClsDetType::ListRow)
                    continue;
                if (!focused && cr.u.lrow.ws == WState::Focused) {
                    focused = &cr;
                    focused_row = row;
                }
                int conf = ocr::ocrRowText(ocr::GENERIC, grayImage, ai::rEnv, cr, "name", cr.text);
                cr.u.lrow.text_confidence = conf;
                if (conf <= ocr_confidence && checkOcrError(cr))
                    rows_with_error.push_back(row);
                row += 1;
            }
            if (!focused) {
                LOG(ERROR) << "Focused row not found";
                failCount += 1;
                if (failCount >= 3)
                    throw_failed("Focused row not found");
                kbd::send("UI_Down", 0, 500);
                kbd::send("UI_Up", 0, 500);
                continue;
            }
            failCount = 0;
            if (focused_row == 0 && offset > 0) {
                LOG(INFO) << "Nav list wrapped at offset " << offset << "; finishing task";
                break;
            }
            if (focused->u.lrow.text_confidence <= ocr_confidence && checkOcrError(*focused)) {
                LOG(INFO) << "Checking nav-point at offset " << offset << ", detected conf=" << focused->u.lrow.text_confidence << "%";
                checkNavPoint(offset);
                offset += 1;
                kbd::send("UI_Down", 0, 100);
                continue;
            }
            LOG(INFO) << "Skip nav-point at offset " << offset << ", detected conf=" << focused->u.lrow.text_confidence << "%";
            int skip_rows = -1;
            for (int bad_row: rows_with_error) {
                if (bad_row > focused_row) {
                    skip_rows = bad_row - focused_row;
                    break;
                }
            }
            if (skip_rows > 0) {
                for (int dn = 0; dn < skip_rows; dn++)
                    kbd::send("UI_Down", 0, 100);
                continue;
            }
            for (int dn = 0; dn < (10 - focused_row) + 8; dn++)
                kbd::send("UI_Down", 0, 100);
            for (int up = 0; up < 8; up++)
                kbd::send("UI_Up", 0, 100);
        }
    }
    return true;
}

bool TaskDebugFindAllNavPoints::checkOcrError(const ClassifiedRect& cr) {
    std::wstring wide = toUtf16(cr.text);
    StationRowInfo rowInfo {};
    if (!parseRowInfo(wide, rowInfo))
        return true;
    gal::NavType* nav_type = nullptr;
    for (auto nt : gal::ALL_NAV_TYPES) {
        if (nt->charOCR == rowInfo.type) {
            nav_type = nt;
            break;
        }
    }
    if (!nav_type)
        return true;
    std::string text = toUtf8(rowInfo.name);
    if (guessBestStation(text, nav_type) < txt_confidence)
        return true;
    return false;
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

json5pp::value TaskDebugFindAllNavPoints::curlGetRequest(const char* base_url) {
    const gal::spStarSystem& ss = gal::getCurrentStarSystem();
    if (!ss || ss->systemName.empty() || !ss->systemAddress)
        return nullptr;

    std::string readBuffer;

    CURL* curl = curl_easy_init();
    if (!curl)
        return {};

    std::string url = base_url;
    if (url.contains("www.edsm.net")) {
        std::string systemName = ss->systemName;
        url += "?systemName=";
        url += curl_easy_escape(curl, systemName.c_str(), systemName.length());
    } else {
        url += std::to_string(ss->systemAddress);
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    if (Cfg.getCurlInsecure()) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYSTATUS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
    }
    if (auto& proxy = Cfg.getCurlProxyURL(); !proxy.empty())
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    char errbuf[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
        LOG(ERROR) << "Curl error: " << errbuf;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return {};

    //LOG(INFO) << "EDSM responce: " << resp.str();
    try {
        auto jresp = json5pp::parse5(readBuffer);
        if (!jresp["record"]) {
            LOG(ERROR) << "Bad response, expecting 'record': " << jresp;
            return {};
        }
        return std::move(jresp["record"]);
    } catch (const json5pp::syntax_error& ex) {
        LOG(ERROR) << "Error parsing EDSM response: " << ex.what();
        return {};
    }
}

json5pp::value TaskDebugFindAllNavPoints::curlPostRequest(const char* base_url, json5pp::value& data) {
    std::string readBuffer;

    CURL* curl = curl_easy_init();
    if (!curl)
        return {};

    // Set URL and perform the request
    curl_easy_setopt(curl, CURLOPT_URL, base_url);

    if (Cfg.getCurlInsecure()) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYSTATUS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY_SSL_VERIFYHOST, 0L);
    }
    if (auto& proxy = Cfg.getCurlProxyURL(); !proxy.empty())
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset: utf-8");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
    std::string payload = data.stringify();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.size());
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    char errbuf[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
        LOG(ERROR) << "Curl error: " << errbuf;

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return {};

    try {
        auto jresp = json5pp::parse5(readBuffer);
        if (!jresp["results"].is_array()) {
            LOG(ERROR) << "Bad response, expecting 'results': " << jresp;
            return nullptr;
        }
        auto& results = jresp.as_object()["results"];
        for (auto& r : results.as_array()) {
            r.as_object().erase("bodies");
            r.as_object().erase("stations");
            r.as_object().erase("minor_faction_presences");
            r.as_object().erase("power");
        }
        return results;
    } catch (const json5pp::syntax_error& ex) {
        LOG(ERROR) << "Error parsing response: " << ex.what();
        return nullptr;
    }

}

// For nearest systems:
// https://www.spansh.co.uk/api/systems/search
// POST /api/systems/search HTTP/1.1
//Accept: */*
//Accept-Encoding: deflate, gzip
//User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36 OPR/120.0.0.0
//Host: www.spansh.co.uk
//Content-Type: application/json
//Content-Length: 156
//
//{
//    "filters": {
//        "distance": {
//            "min": 0, "max": 10
//        }
//    },
//    "size": 100,
//    "page": 0,
//    "reference_system": "Aornum"
//}
//
bool TaskDebugFindAllNavPoints::getSpanishInfo() {
    auto j = curlGetRequest("https://www.spansh.co.uk/api/system/");
    if (!j)
        return false;
    spanishSystemInfo = std::move(j);
    LOG(DEBUG) << "Got system info: " << spanishSystemInfo;

    const gal::spStarSystem& ss = gal::getCurrentStarSystem();
    std::string systemName = ss->systemName;

    json5pp::value payload = json5pp::object({
            { "reference_system", systemName},
            { "filters", json5pp::object({ {"distance", json5pp::object({{"min", 0}, {"max", 20}})} })},
            { "size", 100},
            { "page", 0}
    });

    j = curlPostRequest("https://www.spansh.co.uk/api/systems/search", payload);
    if (!j)
        return false;
    spanishNearSystems = std::move(j);
    LOG(DEBUG) << "Got near systems: " << spanishNearSystems;

    return true;
}
//bool TaskDebugFindAllNavPoints::getSystemStations() {
//    auto j = curlRequest("https://www.edsm.net/api-system-v1/stations");
//    if (!j.contains("stations") || !j["stations"].is_array())
//        return false;
//    LOG(INFO) << "Got system stations: " << j;
//    systemStations = std::move(j["stations"].as_array());
//    return true;
//}
//
//bool TaskDebugFindAllNavPoints::getSystemBodies() {
//    auto j = curlRequest("https://www.edsm.net/api-system-v1/bodies");
//    if (!j.contains("bodies") || !j["bodies"].is_array())
//        return false;
//    systemBodies = std::move(j["bodies"].as_array());
//    return true;
//}


bool TaskDebugFindAllNavPoints::checkNavPoint(int offset) {
    std::string lbl_text;
    std::string lbl_anchor;
    const gal::NavType* navType = nullptr;
    cv::Mat grayImage;
    kbd::send("UI_Select", 0, 1500);
    ai::detectEDStateGrayIm(DetectLevel::Buttons, grayImage);
    for (auto& cr : ai::rEnv.classified) {
        if (cr.cdt == ClsDetType::LineDetected && cr.text.starts_with("nvline:")) {
            lbl_anchor = cr.text.substr(7);
            navType = guessNavType(lbl_text, lbl_anchor);
        }
        if (cr.cdt == ClsDetType::Widget && cr.text == "lbl-title") {
            saveOcrNavigationLbl(grayImage, cr, offset, lbl_text, navType);
        }
    }

    kbd::send("UI_Back", 50, 1000);

    ai::detectEDStateGrayIm(DetectLevel::ListRows, grayImage);
    for (auto& cr : ai::rEnv.classified) {
        if (cr.cdt != ClsDetType::ListRow)
            continue;
        if (cr.u.lrow.ws == WState::Focused) {
            saveOcrNavigationRow(grayImage, cr, offset, lbl_text, navType);
            break;
        }
    }
    return true;
}

const gal::NavType* TaskDebugFindAllNavPoints::guessNavType(const std::string& lbl_name, const std::string& lbl_anchor) const {
    for (auto nt : gal::ALL_NAV_TYPES) {
        if (contains(nt->navIcons, lbl_anchor)) {
            LOG(INFO) << "Guessed type from label '" << lbl_anchor << "'";
            return nt;
        }
    }
    return nullptr;
}

int TaskDebugFindAllNavPoints::guessBestStation(std::string& text, const gal::NavType* nav_type) const {
    const gal::spStarSystem& ss = gal::getCurrentStarSystem();
    // find best station name
    std::string best_name;
    double best_rate = 0;
    FuzzyMatch fm;
    std::wstring text_ocr = fm.toOCR(toUtf16(text));
    TypeNav typeNav = nav_type ? nav_type->type : TypeNav::Other;
    switch (typeNav) {
    case TypeNav::Other:
        break;
    case TypeNav::Body:
    case TypeNav::Star:
    case TypeNav::Planet:
    case TypeNav::Barycenter:
    case TypeNav::Ring:
    case TypeNav::AsteroidCluster:
        if (spanishSystemInfo["bodies"].is_array()) {
            for (auto &js: spanishSystemInfo.at("bodies").as_array()) {
                if (!js["type"].is_string() || !contains(nav_type->typeAliases, js["type"].as_string()))
                    continue;
                std::string name = js.at("name").as_string();
                std::wstring name_ocr = fm.toOCR(toUtf16(name));
                double rate = fm.ratio(text_ocr, name_ocr);
                if (rate > best_rate) {
                    best_rate = rate;
                    best_name = name;
                }
            }
        }
        break;
    case TypeNav::StarSystem:
        if (spanishNearSystems.is_array()) {
            for (auto &js: spanishNearSystems.as_array()) {
                std::string name = js.at("name").as_string();
                std::wstring name_ocr = fm.toOCR(toUtf16(name));
                double rate = fm.ratio(text_ocr, name_ocr);
                if (rate > best_rate) {
                    best_rate = rate;
                    best_name = name;
                }
            }
        }
        break;
    case TypeNav::SpaceStation:
    case TypeNav::Orbis:
    case TypeNav::Ocellus:
    case TypeNav::Dodec:
    case TypeNav::Coriolis:
    case TypeNav::AsteroidBase:
    case TypeNav::SpaceInstallation:
    case TypeNav::SpaceConstrDepot:
    case TypeNav::Megaship:
    case TypeNav::StationMegaShip:
    case TypeNav::FleetCarrier:
    case TypeNav::SquadronCarrier:
    case TypeNav::StrongholdCarrier:
    case TypeNav::ColonisationShip:
    //case TypeNav::TrailblazerDream:
    case TypeNav::PlanetaryThing:
    case TypeNav::PlanetaryStation:
    case TypeNav::PlanetaryPort:
    case TypeNav::EngineerPort:
    case TypeNav::Settlement:
    case TypeNav::PlanetaryInstallation:
    case TypeNav::PlanetaryConstrDepot:
        if (spanishSystemInfo["stations"].is_array()) {
            for (auto &js: spanishSystemInfo.at("stations").as_array()) {
                if (!js["type"].is_string() || !contains(nav_type->typeAliases, js["type"].as_string()))
                    continue;
                std::string name = js.at("name").as_string();
                while (name.ends_with("+"))
                    name = trim(name.substr(0, name.size() - 1));
                std::wstring name_ocr = fm.toOCR(toUtf16(name));
                double rate = fm.ratio(text_ocr, name_ocr);
                if (rate > best_rate) {
                    best_rate = rate;
                    best_name = name;
                }
            }
        }
        break;
    }
    if (best_name.empty())
        best_rate = 0;
    if (best_rate < 60) {
        LOG(WARNING) << "Station not found for: '" << text << "'";
        return best_rate;
    }
    text = best_name;
    LOG(INFO) << "Guessed station name: '" << text << "' with conf rate: " << best_rate;
    return best_rate;
}

void TaskDebugFindAllNavPoints::saveOcrNavigationLbl(const cv::Mat &grayImage, const ClassifiedRect& cr,
                                                     int offset, std::string& lbl_text, const gal::NavType* navType)
{
    lbl_text.clear();
    offset += offset_append;

    std::string filename = std::format("testset-edr/nav-{}-lbl-gray.png", offset);
    if (std::filesystem::exists(filename))
        return;

    std::string text;
    cv::Mat dumpImage;
    int conf = ocr::ocrNavigationLblTextForTraining(grayImage, ai::rEnv, cr, text, dumpImage);
    if (conf < 50) {
        LOG(ERROR) << "Bad ocr for label, conf: " << conf << ", text: "<< text;
        text.clear();
    }
    else if (conf < 93) {
        LOG(INFO) << "Unsure ocr for label, conf: " << conf << ", text: "<< text;
        if (guessBestStation(text, navType) < 60)
            text.clear();
    }
    else {
        LOG(INFO) << "Confident ocr for label, conf: " << conf << ", text: "<< text;
        guessBestStation(text, navType);
    }
    lbl_text = text;

    if (!dump_images)
        return;

    filename = std::format("testset-edr/nav-{}-lbl-gray.png", offset);
    cv::imwrite(filename, dumpImage);

//    cv::Mat otsuImage;
//    cv::threshold(dumpImage, otsuImage, 150, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
//    filename = std::format("testset-edr/nav-{}-lbl-otsu.png", offset);
//    cv::imwrite(filename, otsuImage);

    FuzzyMatch fm;
    std::wstring wide = toUtf16(text);
    std::wstring nameOCR = fm.toOCR(wide);

    filename = std::format("testset-edr/nav-{}-lbl-gray.gt.txt", offset);
    std::ofstream gt_txt(filename, std::ios::trunc | std::ios::binary);
    gt_txt << toUtf8(nameOCR);
    gt_txt.close();

//    filename = std::format("testset-edr/nav-{}-lbl-otsu.gt.txt", offset);
//    gt_txt.open(filename, std::ios::trunc | std::ios::binary);
//    gt_txt << toUtf8(nameOCR);
//    gt_txt.close();
}

bool TaskDebugFindAllNavPoints::parseRowInfo(std::wstring text, StationRowInfo& rowInfo) {
    text = trim(text);
    if (text.empty())
        return false;
    wchar_t ch = text.front();
    if (text[0] >= 0x2000 && text[0] <= 0x2FFF) {
        rowInfo.type = text[0];
        text = trim(text.substr(1));
        if (text.empty())
            return false;
    }
    ch = text.back();
    wchar_t ch1 = text[text.size()-2];
    wchar_t ch2 = text[text.size()-3];
    if (ch == gal::LOCATION_MARK ||
        (ch1 == gal::SHIELD1_MARK || ch1 == gal::SHIELD2_MARK || ch1 == gal::SHIELD3_MARK) ||
        (ch1 == L' ' && (ch2 == gal::SHIELD1_MARK || ch2 == gal::SHIELD2_MARK || ch2 == gal::SHIELD3_MARK))
    ) {
        rowInfo.isLocation = true;
        text.pop_back();
        text = trim(text);
        if (text.empty())
            return false;
    }
    ch = text.back();
    if (ch == gal::SHIELD1_MARK || ch == gal::SHIELD2_MARK || ch == gal::SHIELD3_MARK) {
        rowInfo.danger = ch;
        text.pop_back();
        text = trim(text);
        if (text.empty())
            return false;
    }
    ch = text.back();
    while (ch == L'+') {
        rowInfo.size += 1;
        text.pop_back();
        text = trim(text);
        if (text.empty())
            return false;
        ch = text.back();
    }
    if (text[0] == L'<' && ch == '>') {
        rowInfo.isTarget = true;
        text = text.substr(1,text.size()-2);
        text.pop_back();
        text = trim(text.substr(1));
        if (text.empty())
            return false;
    }
    rowInfo.name = text;
    return true;
}

void TaskDebugFindAllNavPoints::saveOcrNavigationRow(const cv::Mat &grayImage, const ClassifiedRect& cr, int offset,
                                                     const std::string& lbl_text, const gal::NavType* navType)
{
    offset += offset_append;

    std::string filename = std::format("testset-edr/nav-{}-row-gray.png", offset);
    if (std::filesystem::exists(filename))
        return;

    StationRowInfo rowInfo {};
    std::string text;
    cv::Mat dumpImage;
    int conf = ocr::ocrRowTextForTraining(ocr::GENERIC, grayImage, ai::rEnv, cr, "name", text, dumpImage);
    if (conf < 50) {
        LOG(ERROR) << "Bad ocr for nav row, conf: " << conf << ", text: "<< text;
        text.clear();
    }
    else if (conf < 93) {
        LOG(INFO) << "Unsure ocr for nav row, conf: " << conf << ", text: "<< text;
        parseRowInfo(toUtf16(text), rowInfo);
    }
    else {
        LOG(INFO) << "Confident ocr for nav row, conf: " << conf << ", text: "<< text;
        parseRowInfo(toUtf16(text), rowInfo);
    }

    std::string distText;
    cv::Mat distImage;
    conf = ocr::ocrRowTextForTraining(ocr::DISTANCE, grayImage, ai::rEnv, cr, "dist", distText, distImage);
    if (conf < 50) {
        LOG(ERROR) << "Bad ocr for nav dist, conf: " << conf << ", text: "<< distText;
        distText.clear();
    }
    else if (conf < 93) {
        LOG(INFO) << "Unsure ocr for nav dist, conf: " << conf << ", text: "<< distText;
    }
    else {
        LOG(INFO) << "Confident ocr for nav dist, conf: " << conf << ", text: "<< distText;
    }

    if (!dump_images)
        return;

    FuzzyMatch fm;
    std::wstring distOCR = fm.toOCR(toUtf16(distText));
    std::wstring nameOCR = fm.toOCR(toUtf16(lbl_text));
    if (rowInfo.size > 0) {
        nameOCR += L" ";
        for (int i = 0; i < rowInfo.size; i++)
            nameOCR += L"+";
    }
    if (rowInfo.isTarget)
        nameOCR = L"< " + nameOCR + L" > ";
    if (rowInfo.danger)
        nameOCR += rowInfo.danger;
    if (rowInfo.isLocation)
        nameOCR += gal::LOCATION_MARK;
    if (navType)
        nameOCR = navType->charOCR + (L" " + nameOCR);

    filename = std::format("testset-edr/nav-{}-row-gray.png", offset);
    cv::imwrite(filename, dumpImage);
//    cv::Mat rowOtsuImage;
//    cv::threshold(dumpImage, rowOtsuImage, 150, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
//    filename = std::format("testset-edr/nav-{}-row-otsu.png", offset);
//    cv::imwrite(filename, rowOtsuImage);

    filename = std::format("testset-edr/nav-{}-num-gray.png", offset);
    cv::imwrite(filename, distImage);
//    cv::Mat distOtsuImage;
//    cv::threshold(distImage, distOtsuImage, 150, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
//    filename = std::format("testset-edr/nav-{}-num-otsu.png", offset);
//    cv::imwrite(filename, distOtsuImage);

    std::ofstream gt_txt;
    filename = std::format("testset-edr/nav-{}-row-gray.gt.txt", offset);
    gt_txt.open(filename, std::ios::trunc | std::ios::binary);
    gt_txt << toUtf8(nameOCR);
    gt_txt.close();
//    filename = std::format("testset-edr/nav-{}-row-otsu.gt.txt", offset);
//    gt_txt.open(filename, std::ios::trunc | std::ios::binary);
//    gt_txt << toUtf8(nameOCR);
//    gt_txt.close();
    filename = std::format("testset-edr/nav-{}-num-gray.gt.txt", offset);
    gt_txt.open(filename, std::ios::trunc | std::ios::binary);
    gt_txt << toUtf8(distOCR);
    gt_txt.close();
//    filename = std::format("testset-edr/nav-{}-num-otsu.gt.txt", offset);
//    gt_txt.open(filename, std::ios::trunc | std::ios::binary);
//    gt_txt << toUtf8(distOCR);
//    gt_txt.close();
}


} // ai