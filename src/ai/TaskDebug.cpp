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
    std::string dirname = "cache/testset-edr";

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
        throw_failed_("Not in market?");
    std::string marketMode = ai::uiState.path().substr(11);
    std::vector<Commodity*> table;
    if (marketMode == "mod-sell")
        table = Cfg.getMarketInSellOrder();
    else if (marketMode == "mod-buy")
        table = Cfg.getMarketInBuyOrder();
    else
        throw_failed_(std::format("Unknown market mode {}", marketMode));
    if (table.empty())
        throw_failed_("Empty market?");
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
        std::string lbl_filename = std::format("cache/testset-edr/{}-lbl-gray.png", currCommodity->nameId);
        std::string row_filename = std::format("cache/testset-edr/{}-row-gray.png", currCommodity->nameId);
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
    std::string filename = std::format("cache/testset-edr/{}-row-gray.png", commodity->nameId);
    if (std::filesystem::exists(filename))
        return;

    std::string text;
    cv::Mat rowDumpImage;
    int conf = ocr::ocrRowTextForTraining(ocr::GENERIC, grayImage, ai::rEnv, cr, "name", text, rowDumpImage);

    filename = std::format("cache/testset-edr/{}-row-gray.png", commodity->nameId);
    cv::imwrite(filename, rowDumpImage);

//    cv::Mat rowOtsuImage;
//    cv::threshold(rowDumpImage, rowOtsuImage, 150, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
//    filename = std::format("cache/testset-edr/{}-row-otsu.png", commodity->nameId);
//    cv::imwrite(filename, rowOtsuImage);

    FuzzyMatch fm;
    std::wstring nameOCR = fm.toOCR(commodity->wide);

    std::ofstream gt_txt;
    filename = std::format("cache/testset-edr/{}-row-gray.gt.txt", commodity->nameId);
    gt_txt.open(filename, std::ios::trunc | std::ios::binary);
    gt_txt << toUtf8(nameOCR);
    gt_txt.close();
//    filename = std::format("cache/testset-edr/{}-row-otsu.gt.txt", commodity->nameId);
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
    std::string filename = std::format("cache/testset-edr/{}-lbl-gray.png", commodity->nameId);
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
            filename = std::format("cache/testset-edr/{}-lbl{}-gray.png", commodity->nameId, l);
        else
            filename = std::format("cache/testset-edr/{}-lbl-gray.png", commodity->nameId);
        cv::imwrite(filename, lblDumpImages[l]);

//        cv::Mat lblOtsuImage;
//        cv::threshold(lblDumpImages[l], lblOtsuImage, 150, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
//        if (l > 0)
//            filename = std::format("cache/testset-edr/{}-lbl{}-otsu.png", commodity->nameId, l);
//        else
//            filename = std::format("cache/testset-edr/{}-lbl-otsu.png", commodity->nameId);
//        cv::imwrite(filename, lblOtsuImage);

        if (l > 0)
            filename = std::format("cache/testset-edr/{}-lbl{}-gray.gt.txt", commodity->nameId, l);
        else
            filename = std::format("cache/testset-edr/{}-lbl-gray.gt.txt", commodity->nameId);
        std::ofstream gt_txt(filename, std::ios::trunc | std::ios::binary);
        if (textsMatch) {
            gt_txt << toUtf8(dumpTexts[l]);
        } else {
            if (l == 0)
                gt_txt << toUtf8(commodity->wocr);
        }
        gt_txt.close();

//        if (l > 0)
//            filename = std::format("cache/testset-edr/{}-lbl{}-otsu.gt.txt", commodity->nameId, l);
//        else
//            filename = std::format("cache/testset-edr/{}-lbl-otsu.gt.txt", commodity->nameId);
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
    //std::string filename = std::format("cache/testset-edr/nav-{}-lbl-gray.png", lng, offset);
    //std::string filename = std::format("cache/testset-edr/nav-{}-row-gray.png", lng, offset);
    //std::string filename = std::format("cache/testset-edr/nav-{}-num-gray.png", lng, offset);
    std::string dirname = "cache/testset-edr";
    std::filesystem::create_directories(dirname);

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
        throw_failed_("Not in mod-nav-list?");

    if (!getSpanishInfo()) {
        LOG(ERROR) << "Cannot get system info from spansh.co.uk";
        throw_failed_("Cannot get system info from spansh.co.uk");
    }

    if (!(resume || unfocused)) {
        kbd::send("UI_Right");
        kbd::send("UI_Down");
        kbd::send("UI_Up", 3000, 100);
    }

    int offset = 0;
    int failCount=0;
    if (unfocused) {
        cv::Mat grayImage;
        int focusIdx = -1;
        auto rows = nl.recognizeWholePage(grayImage, focusIdx);
        if (rows.empty())
            throw_failed_("Cannot recognize nav list");
        for (auto &nle: nl.list) {
            if (checkOcrError(nle)) {
                LOG_INFO("Checking nav-point at offset {}, ocr conf={}%, text conf={}%",
                                         offset, nle.ocr_conf, nle.txt_conf);
                saveOcrNavigationRow(grayImage, *rows[nle.index], offset, nle);
                offset += 1;
            } else {
                LOG_INFO("Skip nav-point at offset {}, ocr conf={}%, text conf={}%: {} {}",
                                         offset, nle.ocr_conf, nle.txt_conf,
                                         toUtf8(&nle.icon, 1), toUtf8(nle.name));
            }
        }
    } else {
        for (bool first=true;; first=false) {
            cv::Mat grayImage;
            int focusIdx = 0;
            auto rows = nl.initNavList(grayImage, focusIdx);
            if (rows.empty()) {
                LOG(ERROR) << "Cannot recognize nav list";
                //throw_failed_("Cannot recognize nav list");
                kbd::send("UI_Down", 0, 100);
                continue;
            }
            nl.parseNavRow(grayImage, ai::rEnv, *rows[focusIdx], focusIdx);
            if (focusIdx == 0 && !first) {
                LOG_INFO("Nav list wrapped at offset {}; finishing task", offset);
                break;
            }
            nl.guessNavItem(focusIdx);
            auto& nle = nl.list[focusIdx];
            if (checkOcrError(nle)) {
                LOG_INFO("Checking nav-point at offset {}, ocr conf={}%, text conf={}%",
                                         offset, nle.ocr_conf, nle.txt_conf);
                saveOcrNavigationRow(grayImage, *rows[nle.index], offset, nle);
                offset += 1;
                kbd::send("UI_Down", 0, 100);
                continue;
            }
            LOG_INFO("Skip nav-point at offset {}, ocr conf={}%",
                                     offset, nle.ocr_conf, nle.txt_conf);
            kbd::send("UI_Down", 0, 100);
        }
    }
    return true;
}

bool TaskDebugFindAllNavPoints::checkOcrError(const NavListEntry& nle) {
    if (nle.icon == 0 || nle.icon == gal::ERROR_MARK)
        return true;
    if (nle.ocr_conf < ocr_confidence)
        return true;
    // ignore construction depots
    if (nle.item && (nle.item->type == TypeNav::PlanetaryConstrDepot || nle.item->type == TypeNav::SpaceConstrDepot))
        return false;
    if (nle.txt_conf < txt_confidence)
        return true;
    return false;
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

js::value TaskDebugFindAllNavPoints::curlGetRequest(const char* base_url) {
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
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
        auto jresp = js::parse5(readBuffer);
        if (!jresp["record"]) {
            LOG(ERROR) << "Bad response, expecting 'record': " << jresp;
            return {};
        }
        return std::move(jresp["record"]);
    } catch (const js::syntax_error& ex) {
        LOG(ERROR) << "Error parsing EDSM response: " << ex.what();
        return {};
    }
}

js::value TaskDebugFindAllNavPoints::curlPostRequest(const char* base_url, js::value& data) {
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
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
        auto jresp = js::parse5(readBuffer);
        if (!jresp["results"].is_array()) {
            LOG(ERROR) << "Bad response, expecting 'results': " << jresp;
            return nullptr;
        }
        auto& results = jresp["results"].deref();
        for (auto& r : results.as_array()) {
            r["bodies"] = nullptr;
            r["stations"] = nullptr;
            r["minor_faction_presences"] = nullptr;
            r["power"] = nullptr;
        }
        return results;
    } catch (const js::syntax_error& ex) {
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
//    auto j = curlGetRequest("https://www.spansh.co.uk/api/system/");
//    if (!j)
//        return false;
//    spanshSystemInfo = std::move(j);
//    LOG_DEBUG("Got system info: {}", spanshSystemInfo);

    const gal::spStarSystem& ss = gal::getCurrentStarSystem();
    std::string systemName = ss->systemName;

    js::value payload = js::object({
            { "reference_system", systemName},
            { "filters",          js::object({{"distance", js::object({{"min", 0}, {"max", 20}})} })},
            { "size",             100},
            { "page",             0}
    });

    auto jn = curlPostRequest("https://www.spansh.co.uk/api/systems/search", payload);
    if (!jn.is_array())
        return false;
    std::vector<std::string> systems;
    for (auto& s : jn.as_array())
        systems.push_back(s["name"].as_string());
    nl.setNearestSystems(systemName, systems);
    spanshNearSystems = std::move(jn);
    LOG_DEBUG("Got near systems: {}", spanshNearSystems);

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


//bool TaskDebugFindAllNavPoints::checkNavPoint(int offset) {
//    kbd::send("UI_Select");
//    kbd::send("UI_Select");
//    sleep(1000);
//    std::string name = st::destination.name;
//    kbd::send("UI_Select");
//    kbd::send("UI_Select");
//    saveOcrNavigationRow(grayImage, cr, offset, lbl_text, navType);
//    return true;
//}

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
        if (spanshSystemInfo["bodies"].is_array()) {
            for (auto &js: spanshSystemInfo.at("bodies").as_array()) {
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
        if (spanshNearSystems.is_array()) {
            for (auto &js: spanshNearSystems.as_array()) {
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
    case TypeNav::PlanetaryThing:
    case TypeNav::PlanetaryPort:
    case TypeNav::EngineerPort:
    case TypeNav::Settlement:
    case TypeNav::PlanetaryInstallation:
    case TypeNav::PlanetaryConstrDepot:
        if (spanshSystemInfo["stations"].is_array()) {
            for (auto &js: spanshSystemInfo.at("stations").as_array()) {
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
        LOG_WARNING("Station not found for: '{}'", text);
        return best_rate;
    }
    text = best_name;
    LOG_INFO("Guessed station name: '{}' with conf rate: {}", text, best_rate);
    return best_rate;
}

void TaskDebugFindAllNavPoints::saveOcrNavigationLbl(const cv::Mat &grayImage, const ClassifiedRect& cr,
                                                     int offset, std::string& lbl_text, const gal::NavType* navType)
{
    lbl_text.clear();
    offset += offset_append;

    std::string filename = std::format("cache/testset-edr/nav-{}-lbl-gray.png", offset);
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

    filename = std::format("cache/testset-edr/nav-{}-lbl-gray.png", offset);
    cv::imwrite(filename, dumpImage);

//    cv::Mat otsuImage;
//    cv::threshold(dumpImage, otsuImage, 150, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
//    filename = std::format("cache/testset-edr/nav-{}-lbl-otsu.png", offset);
//    cv::imwrite(filename, otsuImage);

    FuzzyMatch fm;
    std::wstring wide = toUtf16(text);
    std::wstring nameOCR = fm.toOCR(wide);

    filename = std::format("cache/testset-edr/nav-{}-lbl-gray.gt.txt", offset);
    std::ofstream gt_txt(filename, std::ios::trunc | std::ios::binary);
    gt_txt << toUtf8(nameOCR);
    gt_txt.close();

//    filename = std::format("cache/testset-edr/nav-{}-lbl-otsu.gt.txt", offset);
//    gt_txt.open(filename, std::ios::trunc | std::ios::binary);
//    gt_txt << toUtf8(nameOCR);
//    gt_txt.close();
}

void TaskDebugFindAllNavPoints::saveOcrNavigationRow(
        const cv::Mat &grayImage, const ClassifiedRect& cr, int offset, const NavListEntry& nle)
{
    offset += offset_append;

    std::string filename = std::format("cache/testset-edr/nav-{}-row-gray.png", offset);
    if (std::filesystem::exists(filename))
        return;

    std::string nameText;
    cv::Mat dumpImage;
    int conf = ocr::ocrRowTextForTraining(ocr::GENERIC, grayImage, ai::rEnv, cr, "name", nameText, dumpImage);
    if (conf < 50) {
        LOG(ERROR) << "Bad ocr for nav row, conf: " << conf << ", text: "<< nameText;
        nameText.clear();
    }
    else if (conf < ocr_confidence) {
        LOG(INFO) << "Unsure ocr for nav row, conf: " << conf << ", text: "<< nameText;
    }
    else {
        LOG(INFO) << "Confident ocr for nav row, conf: " << conf << ", text: "<< nameText;
    }
    if (nle.item)
        nameText = nle.item->name;

    std::string distText;
    cv::Mat distImage;
    int distConf = ocr::ocrRowTextForTraining(ocr::DISTANCE, grayImage, ai::rEnv, cr, "dist", distText, distImage);
    if (distConf < 50) {
        LOG(ERROR) << "Bad ocr for nav dist, conf: " << distConf << ", text: "<< distText;
        distText.clear();
    }
    else if (distConf < ocr_confidence) {
        LOG(INFO) << "Unsure ocr for nav dist, conf: " << distConf << ", text: "<< distText;
    }
    else {
        LOG(INFO) << "Confident ocr for nav dist, conf: " << distConf << ", text: "<< distText;
    }
    dist_t dist = parseDist(toUtf16(distText), distConf);
    bool save_dist = !dist.valid() || distConf < ocr_confidence || dist.conf < txt_confidence;
    if (distImage.empty())
        save_dist = false;

    if (!dump_images)
        return;

    FuzzyMatch fm;
    std::wstring distOCR = fm.toOCR(toUtf16(distText));
    std::wstring nameOCR = fm.toOCR(toUtf16(nameText));
    if (nle.portSize > 0) {
        nameOCR += L" ";
        for (int i = 0; i < nle.portSize; i++)
            nameOCR += L"+";
    }
    if (nle.isTarget)
        nameOCR = L"< " + nameOCR + L" > ";
    if (nle.portDanger == 1) nameOCR += gal::SHIELD1_MARK;
    if (nle.portDanger == 2) nameOCR += gal::SHIELD2_MARK;
    if (nle.portDanger == 3) nameOCR += gal::SHIELD3_MARK;
    if (nle.isMarked)
        nameOCR += gal::LOCATION_MARK;
    gal::NavType* nt = nle.item ? gal::NavType::findNavType(nle.item->type) : nullptr;
    if (nt) {
        if (nle.item->type == TypeNav::Planet) {
            if (nle.item->special)
                nameOCR = gal::LAND.charOCR + (L" " + nameOCR);
            else
                nameOCR = gal::BODY.charOCR + (L" " + nameOCR);
        }
        nameOCR = nt->charOCR + (L" " + nameOCR);
    }
    else if (nle.icon)
        nameOCR = nle.icon + (L" " + nameOCR);

    filename = std::format("cache/testset-edr/nav-{}-row-gray.png", offset);
    cv::imwrite(filename, dumpImage);

    if (save_dist) {
        filename = std::format("cache/testset-edr/nav-{}-num-gray.png", offset);
        cv::imwrite(filename, distImage);
    }

    std::ofstream gt_txt;
    filename = std::format("cache/testset-edr/nav-{}-row-gray.gt.txt", offset);
    gt_txt.open(filename, std::ios::trunc | std::ios::binary);
    gt_txt << toUtf8(nameOCR);
    gt_txt.close();

    if (save_dist) {
        filename = std::format("cache/testset-edr/nav-{}-num-gray.gt.txt", offset);
        gt_txt.open(filename, std::ios::trunc | std::ios::binary);
        gt_txt << toUtf8(distOCR);
        gt_txt.close();
    }
}


} // ai