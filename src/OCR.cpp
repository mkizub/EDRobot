//
// Created by mkizub on 22.07.2025.
//

#include "pch.h"

#include "OCR.h"
#include "EDWidget.h"
#include "FuzzyMatch.h"

#include <tesseract/baseapi.h>
#include <tesseract/capi.h>
#include <leptonica/allheaders.h>

namespace ocr {

static std::mutex tesseractMutex;

static tesseract::TessBaseAPI* tesseractApi;
//static tesseract::TessBaseAPI* tesseractApiTmp;
//static cv::dnn_superres::DnnSuperResImpl dnnSuperRes;

bool init(const std::string& tessdata) {
    LOG(INFO) << "Initializing Tesseract OCR for lang '" << enum_name<Lang>(st::lng) << "', tessdata: " << tessdata << "";

    std::set<std::wstring> words_set;
    std::filesystem::path twpath(std::format("tesseract-words-{}.txt", enum_name<Lang>(st::lng)));
    if (std::filesystem::exists(twpath)) {
        FuzzyMatch fm;
        std::ifstream ifs_tw(twpath);
        std::string word;
        while (ifs_tw >> word) {
            std::wstring ocr_word = fm.toOCR(toUtf16(word));
            words_set.insert(ocr_word);
        }
        ifs_tw.close();
        std::ofstream ofs_tw("cache/tesseract-words.txt", std::ios::trunc | std::ios::binary);
        for (auto& w : words_set)
            ofs_tw << toUtf8(w) << std::endl;
        ofs_tw.close();
    }
    std::ofstream ofs_tw("cache/tesseract-words.txt", std::ios::trunc | std::ios::binary);
    for (auto& w : words_set)
        ofs_tw << toUtf8(w) << std::endl;
    ofs_tw.close();

    const char* tesseractLang = "edr";
    std::vector<std::string> vars_vec { "user_words_file", "tessedit_do_invert"};
    std::vector<std::string> vars_values { "cache/tesseract-words.txt", "0" };
    tesseractApi = new tesseract::TessBaseAPI();
    int fail = tesseractApi->Init(tessdata.c_str(), tesseractLang, tesseract::OEM_DEFAULT, nullptr, 0,
                                  &vars_vec, &vars_values, true);
    if (fail) {
        LOG(ERROR) << "Error: Could not initialize tesseract.";
        shutdown();
        return false;
    }
    tesseractApi->SetVariable("user_words_file", "cache/tesseract-words.txt");
    tesseractApi->SetPageSegMode(tesseract::PSM_SINGLE_LINE); // PSM_RAW_LINE
    tesseractApi->SetVariable("tessedit_do_invert", "0");
    return true;
}

void shutdown() {
    LOG(INFO) << "Shutdown Tesseract OCR";
    std::scoped_lock<std::mutex> lock(tesseractMutex);
    if (tesseractApi) {
        tesseractApi->End();
        delete tesseractApi;
        tesseractApi = nullptr;
    }
//    if (tesseractApiTmp) {
//        tesseractApiTmp->End();
//        delete tesseractApiTmp;
//        tesseractApiTmp = nullptr;
//    }
}

int ocrLine(TextType type, const char* dbg, const cv::Mat& grayImage, std::string& text, cv::Rect* rectOut) {
    text.clear();
    if (rectOut)
        *rectOut = {};
    std::scoped_lock<std::mutex> lock(tesseractMutex);
    if (!tesseractApi)
        return 0;
    //assert (grayImage.rows == ocr::LINE_HEIGHT);
    assert (grayImage.type() == CV_8UC1);

    // 'edr'
    auto startTime = std::chrono::high_resolution_clock::now();

    if (type == NUMERIC) {
        tesseractApi->SetVariable("tessedit_char_whitelist", " +-.,/%0123456789");
        tesseractApi->SetPageSegMode(tesseract::PSM_RAW_LINE);
    }
    else if (type == DISTANCE) {
        if (st::lng == Lang::RU)
            tesseractApi->SetVariable("tessedit_char_whitelist", " .,/%0123456789Mмкcвл");
        else
            tesseractApi->SetVariable("tessedit_char_whitelist", " .,/%0123456789Mmklsy");
        tesseractApi->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    }
    else if (type == GENERIC_RAW) {
        tesseractApi->SetVariable("tessedit_char_whitelist", "");
        tesseractApi->SetPageSegMode(tesseract::PSM_RAW_LINE);
    }
    else {
        tesseractApi->SetVariable("tessedit_char_whitelist", "");
        tesseractApi->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    }
    tesseractApi->SetImage(grayImage.data, grayImage.cols, grayImage.rows, 1, (int)grayImage.step);
    tesseractApi->Recognize(nullptr);
    int conf = tesseractApi->MeanTextConf();
    if (conf != 0) {
//        {
//            char *word{};
//            word = tesseractApi->GetUTF8Text();
//            std::wstring whole = toUtf16(word);
//            TessDeleteText(word);
//        }
        bool valid = true;
        tesseract::ResultIterator* ri = tesseractApi->GetIterator();
        cv::Rect lr;
        conf = 0;
        int count = 0;
        do {
            char *word = ri->GetUTF8Text(tesseract::PageIteratorLevel::RIL_WORD);
            int left, top, right, bottom;
            ri->BoundingBox(tesseract::PageIteratorLevel::RIL_WORD, 1, &left, &top, &right, &bottom);
            cv::Rect wr = {left, top, right - left, bottom - top};
            if (lr.empty()) {
                lr = wr;
            } else {
                if (wr.x - (lr.x + lr.width) > 40 && lr.width > 100)
                    valid = false;
                else
                    lr |= wr;
            }
            if (valid) {
                if (!text.empty())
                    text.push_back(' ');
                text += word;
                count += 1;
                conf += ri->Confidence(tesseract::PageIteratorLevel::RIL_WORD);
            }
            TessDeleteText(word);
        } while (valid && ri->Next(tesseract::PageIteratorLevel::RIL_WORD));
        TessPageIteratorDelete(ri);
        if (rectOut)
            *rectOut = lr;
        conf /= count;
//        if (conf < 60)
//            LOG(WARNING) << "Low conf " << conf;
        //const char *outText = tesseractApi->GetUTF8Text();
        //text = trim(outText);
        //delete[] outText;
        //if (rectOut) {
        //    int left, top, right, bottom;
        //    tesseract::ResultIterator* ri = tesseractApi->GetIterator();
        //    if (ri->BoundingBox(tesseract::PageIteratorLevel::RIL_TEXTLINE, 1, &left, &top, &right, &bottom))
        //        *rectOut = {left, top, right - left, bottom - top};
        //    delete ri;
        //}
    }
    tesseractApi->Clear();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    LOG(INFO) << "OCR Output ("<<dbg<<"): '" << text << "' words conf=" << conf << "% took=" << elapsedTime.count() << "us";

    // 'edr-s' (1.25 times faster)
//    startTime = std::chrono::high_resolution_clock::now();
//
//    std::string textTmp;
//    tesseractApiTmp->SetImage(grayImage.data, grayImage.cols, grayImage.rows, 1, (int)grayImage.step);
//    tesseractApiTmp->Recognize(nullptr);
//    int confTmp = tesseractApiTmp->MeanTextConf();
//    if (confTmp != 0) {
//        const char *outText = tesseractApiTmp->GetUTF8Text();
//        textTmp = trim(outText);
//        delete[] outText;
//        if (rect) {
//            int left, top, right, bottom;
//            tesseract::ResultIterator* ri = tesseractApiTmp->GetIterator();
//            if (ri->BoundingBox(tesseract::PageIteratorLevel::RIL_TEXTLINE, 1, &left, &top, &right, &bottom))
//                *rect = {left, top, right - left, bottom - top};
//            delete ri;
//        }
//    }
//    tesseractApiTmp->Clear();
//
//    endTime = std::chrono::high_resolution_clock::now();
//    elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
//
//    LOG(INFO) << "OCR Output (edr-s): '" << textTmp << "' words conf=" << confTmp << "% took=" << elapsedTime.count() << "us";

    return conf;
}

static cv::Mat scaleImage(cv::Mat& image, double scale, bool force) {
    if (scale == 1) {
        if (!force)
            return image;
        cv::Mat scaledImage;
        image.copyTo(scaledImage);
        return scaledImage;
    }
    /*if (scale < 1 || image.channels() != 3)*/ {
        cv::Mat scaledImage;
        cv::resize(image, scaledImage, {0,0}, scale, scale, cv::INTER_CUBIC);
        return scaledImage;
    }

//    // EDSR (x2)
//    cv::Mat edsrImage;
//    dnnSuperRes.upsample(image, edsrImage);
//
//    cv::Mat scaledImage;
//    cv::resize(edsrImage, scaledImage, {0,0}, scale*0.5, scale*0.5, cv::INTER_CUBIC);
//    return scaledImage;
}

int tryOcrRowText(TextType tt, const cv::Mat& ocrImage, std::string& text, cv::Rect* rectOut) {
    {
        int histSize = 256;
        float range[]{0, 256}; //the upper boundary is exclusive
        const float *histRange[]{range};
        cv::Mat hist;
        cv::calcHist(&ocrImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
        cv::GaussianBlur(hist, hist, cv::Size(9,9), 0); // TODO: maybe not needed

//        // Create histogram image
//        int hist_w = 256;
//        int hist_h = 200;
//        int bin_w = cvRound((double)hist_w / 256.0);
//        cv::Mat histImage(hist_h, hist_w, CV_8UC3, cv::Scalar(0, 0, 0));
//        // Normalize histogram to fit image height
//        cv::normalize(hist, hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat());
//        // Draw lines for each bin
//        for (int i = 1; i < histSize; i++) {
//            cv::line(histImage,
//                     cv::Point(bin_w * (i - 1), hist_h - cvRound(hist.at<float>(i - 1))),
//                     cv::Point(bin_w * (i), hist_h - cvRound(hist.at<float>(i))),
//                     cv::Scalar(255, 255, 255), 2, 8, 0); // White lines for grayscale
//        }

        int blackIdx=-1, whiteIdx=-1;
        float blackVal=0, whiteVal=0;
        // first, locate maximum, the background - white value
        for (int i=0; i < 255; i++) {
            float val = hist.at<float>(i);
            if (blackIdx < 0 && val > 0) {
                blackIdx = i;
            }
            if (val > whiteVal) {
                whiteVal = val;
                whiteIdx = i;
            }
        }
        // scale image range (between black and white) to full range
        whiteIdx -= 4;
        blackIdx += 10;
        double mul = 255.0 / (whiteIdx - blackIdx);
        double add = - blackIdx * mul;
        cv::convertScaleAbs(ocrImage, ocrImage, mul, add);
    }
    int conf = ocr::ocrLine(tt, "(list row)", ocrImage, text, rectOut);
    return conf;
}

int ocrRowText(TextType tt, const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, int tab, std::string& text, cv::Rect* rectOut) {
    assert (cr.cdt == ClsDetType::ListRow);
    const widget::List* lst = cr.u.lrow.list;
    if (lst->tabs.size() <= tab)
        return 0;
    auto& t = lst->tabs[tab];
    if (t.tab_right <= t.tab_left || t.ocr_bot <= 0)
        return 0;
    double scale = (ocr::ASCENT+ocr::DESCENT) / double(t.ocr_bot-t.ocr_top) / rEnv.getScale();
    cv::Rect capturedRect = cr.u.lrow.capturedRect;
    capturedRect.x += int(t.tab_left * rEnv.getScale());
    capturedRect.width = int((t.tab_right - t.tab_left) * rEnv.getScale());
    capturedRect &= cv::Rect(0,0,grayImage.cols,grayImage.rows);
    cv::Mat rowImage(grayImage, capturedRect);
    cv::Mat scaledImage = scaleImage(rowImage, scale, cr.u.lrow.ws != WState::Focused);
    if (cr.u.lrow.ws != WState::Focused)
        cv::bitwise_not(scaledImage, scaledImage);
    cv::Rect cropRect {0, 0, scaledImage.cols, scaledImage.rows};
    struct TryOCR {
        std::string text;
        cv::Rect rect;
        int conf;
    };
    std::vector<TryOCR> tries;
    int ocr_top = t.ocr_top * scale - ocr::LEADING;
    cv::Rect ocrRect {0, ocr_top-3, scaledImage.cols, ocr::LINE_HEIGHT+6};
    cv::Mat ocrImage(scaledImage, ocrRect & cropRect);
    TryOCR main = {};
    main.conf = tryOcrRowText(tt, scaledImage(ocrRect & cropRect), main.text, &main.rect);
    if (main.conf >= 90) {
        text = main.text;
        if (rectOut)
            *rectOut = main.rect;
        return main.conf;
    }
    tries.push_back(main);
    if (ocrRect.y > 2) {
        TryOCR above = {};
        above.conf = tryOcrRowText(tt, scaledImage((ocrRect + cv::Point(0,-3)) & cropRect), above.text, &above.rect);
        tries.push_back(above);
    }
    if ((ocrRect.y+ocrRect.height) < cropRect.height-2) {
        TryOCR below = {};
        below.conf = tryOcrRowText(tt, scaledImage((ocrRect + cv::Point(0,+3)) & cropRect), below.text, &below.rect);
        tries.push_back(below);
    }
    int bestIdx = 0;
    for (int i=1; i < tries.size(); i++) {
        if (tries[i].conf > tries[bestIdx].conf)
            bestIdx = i;
    }
    text = tries[bestIdx].text;
    if (rectOut)
        *rectOut = tries[bestIdx].rect;
    return tries[bestIdx].conf;
}

int ocrRowTextForTraining(TextType tt, const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, int tab, std::string& text, cv::Mat& dumpImage) {
    assert (cr.cdt == ClsDetType::ListRow);
    const widget::List* lst = cr.u.lrow.list;
    if (lst->tabs.size() <= tab)
        return 0;
    auto& t = lst->tabs[tab];
    if (t.tab_right <= t.tab_left || t.ocr_bot <= 0)
        return 0;
    double scale = (ocr::ASCENT+ocr::DESCENT) / double(t.ocr_bot-t.ocr_top) / rEnv.getScale();
    cv::Rect capturedRect = cr.u.lrow.capturedRect;
    capturedRect.x += t.tab_left * rEnv.getScale();
    capturedRect.width = (t.tab_right - t.tab_left) * rEnv.getScale();
    cv::Mat rowImage(grayImage, capturedRect);
    cv::Mat scaledImage = scaleImage(rowImage, scale, cr.u.lrow.ws != WState::Focused);
    if (cr.u.lrow.ws != WState::Focused)
        cv::bitwise_not(scaledImage, scaledImage);
    int ocr_top = t.ocr_top * scale - ocr::LEADING;
    if (ocr_top+ocr::LINE_HEIGHT >= scaledImage.rows)
        ocr_top = scaledImage.rows - ocr::LINE_HEIGHT;
    cv::Rect ocrRect {0, ocr_top, scaledImage.cols, ocr::LINE_HEIGHT};
    assert (ocr_top >= 0 && ocr_top+ocr::LINE_HEIGHT <= scaledImage.rows);
    ocrRect &= cv::Rect(0, 0, scaledImage.cols, scaledImage.rows);
    cv::Mat ocrImage(scaledImage, ocrRect);
    cv::Rect rect;
    int conf = ocr::ocrLine(tt, "(list row training)", ocrImage, text, &rect);
    if (conf < 50) {
        dumpImage = ocrImage;
    } else {
        dumpImage = cv::Mat(ocrImage, {rect.x, 0, rect.width, ocrImage.rows});
    }
    return conf;
}

int ocrMarketLblText(const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, std::string& text) {
    assert (cr.cdt == ClsDetType::Widget);
    assert (cr.u.widg.widget->tp == widget::WidgetType::Label);
    const widget::Label* lbl = (const widget::Label*)cr.u.widg.widget;
    double scale = (ocr::ASCENT+ocr::DESCENT) / double(lbl->ocr_bot-lbl->ocr_top) / rEnv.getScale();

    cv::Rect lblCapturedRect = rEnv.cvtReferenceToCaptured(cr.detectedRect);
    cv::Mat lblImage(grayImage, lblCapturedRect);

    cv::Mat scaledImage = scaleImage(lblImage, scale, true);
    cv::bitwise_not(scaledImage, scaledImage);

    int lines = (int)std::round(double(scaledImage.rows) / double(ocr::LINE_HEIGHT));
    int ocr_conf_sum = 0;
    for (int l=0; l < lines; l++) {
        cv::Rect ocrLineRect (0, l*ocr::LINE_HEIGHT, scaledImage.cols, ocr::LINE_HEIGHT);
        cv::Mat ocrImage(scaledImage, ocrLineRect);
        std::string line;
        int conf = ocr::ocrLine(TextType::GENERIC, "(market lbl)", ocrImage, line, nullptr);
        ocr_conf_sum += conf;
        if (l > 0)
            text += " ";
        text += line;
    }
    int ocr_conf = ocr_conf_sum / lines;
    return ocr_conf;
}

int ocrMarketLblTextForTraining(const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, std::vector<std::string>& texts, std::vector<cv::Mat>& dumpImages) {
    assert (cr.cdt == ClsDetType::Widget);
    assert (cr.u.widg.widget->tp == widget::WidgetType::Label);
    const widget::Label* lbl = (const widget::Label*)cr.u.widg.widget;
    double scale = (ocr::ASCENT+ocr::DESCENT) / double(lbl->ocr_bot-lbl->ocr_top) / rEnv.getScale();

    cv::Rect lblCapturedRect = rEnv.cvtReferenceToCaptured(cr.detectedRect);
    cv::Mat lblImage(grayImage, lblCapturedRect);

    cv::Mat scaledImage = scaleImage(lblImage, scale, true);
    cv::bitwise_not(scaledImage, scaledImage);

    int lines = (int)std::round(double(scaledImage.rows) / double(ocr::LINE_HEIGHT));
    int ocr_conf_sum = 0;
    for (int l=0; l < lines; l++) {
        cv::Rect ocrLineRect (0, l*ocr::LINE_HEIGHT, scaledImage.cols, ocr::LINE_HEIGHT);
        cv::Mat ocrImage(scaledImage, ocrLineRect);
        std::string text;
        cv::Rect rect;
        int conf = ocr::ocrLine(TextType::GENERIC, "(market lbl training)", ocrImage, text, &rect);
        ocr_conf_sum += conf;
        texts.push_back(text);
        if (conf < 50) {
            dumpImages.push_back(ocrImage);
        } else {
            dumpImages.push_back(cv::Mat(ocrImage, {rect.x, 0, rect.width, ocrImage.rows}));
        }
    }
    int ocr_conf = ocr_conf_sum / lines;
    return ocr_conf;
}

int ocrNavigationLblText(const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, std::string& text) {
    assert (cr.cdt == ClsDetType::Widget);
    assert (cr.u.widg.widget->tp == widget::WidgetType::Label);
    const widget::Label* lbl = (const widget::Label*)cr.u.widg.widget;
    double scale = (ocr::ASCENT+ocr::DESCENT) / double(lbl->ocr_bot-lbl->ocr_top) / rEnv.getScale();

    cv::Rect lblCapturedRect = rEnv.cvtReferenceToCaptured(cr.detectedRect);
    cv::Mat lblImage(grayImage, lblCapturedRect);

    cv::Mat scaledImage = scaleImage(lblImage, scale, true);
    cv::bitwise_not(scaledImage, scaledImage);

    int conf = ocr::ocrLine(TextType::GENERIC, "(nav lbl)", scaledImage, text, nullptr);
    return conf;
}

int ocrNavigationLblTextForTraining(const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, std::string& text, cv::Mat& dumpImage) {
    assert (cr.cdt == ClsDetType::Widget);
    assert (cr.u.widg.widget->tp == widget::WidgetType::Label);
    const widget::Label* lbl = (const widget::Label*)cr.u.widg.widget;
    double scale = (ocr::ASCENT+ocr::DESCENT) / double(lbl->ocr_bot-lbl->ocr_top) / rEnv.getScale();

    cv::Rect lblCapturedRect = rEnv.cvtReferenceToCaptured(cr.detectedRect);
    cv::Mat lblImage(grayImage, lblCapturedRect);

    cv::Mat scaledImage = scaleImage(lblImage, scale, true);
    cv::bitwise_not(scaledImage, scaledImage);

    cv::Rect rect;
    int conf = ocr::ocrLine(TextType::GENERIC, "(nav lbl training)", scaledImage, text, &rect);
    if (conf < 50) {
        dumpImage = scaledImage;
    } else {
        dumpImage = cv::Mat(scaledImage, {rect.x, 0, rect.width, scaledImage.rows});
    }

    return conf;
}

cv::Mat normalizeTargetDistText(const cv::Mat& grayImage) {
    int histSize = 256;
    float range[]{0, 256}; //the upper boundary is exclusive
    const float *histRange[]{range};
    cv::Mat hist;
    cv::calcHist(&grayImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
    cv::GaussianBlur(hist, hist, cv::Size(9,9), 0); // TODO: maybe not needed

//    // Create histogram image
//    int hist_w = 256;
//    int hist_h = 200;
//    int bin_w = cvRound((double)hist_w / 256.0);
//    cv::Mat histImage(hist_h, hist_w, CV_8UC3, cv::Scalar(0, 0, 0));
//    // Normalize histogram to fit image height
//    cv::normalize(hist, hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat());
//    // Draw lines for each bin
//    for (int i = 1; i < histSize; i++) {
//        cv::line(histImage,
//                 cv::Point(bin_w * (i - 1), hist_h - cvRound(hist.at<float>(i - 1))),
//                 cv::Point(bin_w * (i), hist_h - cvRound(hist.at<float>(i))),
//                 cv::Scalar(255, 255, 255), 2, 8, 0); // White lines for grayscale
//    }

    int blackIdx=-1, whiteIdx=-1;
    float blackVal=0, whiteVal=0;
    // first, locate maximum, the background - white value
    for (int i=10; i < 240; i++) {
        float val = hist.at<float>(i);
        if (blackIdx < 0 && val > 0) {
            blackIdx = i;
        }
        if (val > 0) {
            whiteVal = val;
            whiteIdx = i;
        }
    }
    // scale image range (between black and white) to full range
    double mul = 255.0 / (whiteIdx - blackIdx);
    double add = - blackIdx * mul;
    cv::Mat ocrImage;
    cv::convertScaleAbs(grayImage, ocrImage, mul, add);
    return ocrImage;
}

int ocrTargetDistText(const cv::Mat& grayImage, std::string& text) {
    cv::Mat ocrImage = normalizeTargetDistText(grayImage);
    int conf = ocr::ocrLine(ocr::DISTANCE, "(nav dist)", ocrImage, text, nullptr);
    return conf;
}

int ocrTileLblText(const cv::Mat& grayImage, WState ws, std::string& text) {
    double scale = (ocr::ASCENT+ocr::DESCENT) / double(grayImage.rows);
    cv::Mat scaledImage = scaleImage(const_cast<cv::Mat&>(grayImage), scale, true);
    if (ws != WState::Focused)
        cv::bitwise_not(scaledImage, scaledImage);
    cv::Mat ocrImage = normalizeTargetDistText(scaledImage);
    int conf = ocr::ocrLine(ocr::GENERIC, "(tile text)", ocrImage, text, nullptr);
    return conf;
}

cv::Mat normalizeDetectorText(const cv::Mat& grayImage) {
    int histSize = 256;
    float range[]{0, 256}; //the upper boundary is exclusive
    const float *histRange[]{range};
    cv::Mat hist;
    cv::calcHist(&grayImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
    cv::GaussianBlur(hist, hist, cv::Size(9,9), 0); // TODO: maybe not needed

    // Create histogram image
    int hist_w = 256;
    int hist_h = 200;
    int bin_w = cvRound((double)hist_w / 256.0);
    cv::Mat histImage(hist_h, hist_w, CV_8UC3, cv::Scalar(0, 0, 0));
    // Normalize histogram to fit image height
    cv::normalize(hist, hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat());
    // Draw lines for each bin
    for (int i = 1; i < histSize; i++) {
        cv::line(histImage,
                 cv::Point(bin_w * (i - 1), hist_h - cvRound(hist.at<float>(i - 1))),
                 cv::Point(bin_w * (i), hist_h - cvRound(hist.at<float>(i))),
                 cv::Scalar(255, 255, 255), 2, 8, 0); // White lines for grayscale
    }

    int blackIdx=-1, whiteIdx=-1;
    float blackVal=0, whiteVal=0;
    // first, locate maximum, the background - white value
    for (int i=10; i < 240; i++) {
        float val = hist.at<float>(i);
        if (blackIdx < 0 && val > 0) {
            blackIdx = i;
        }
        if (val > 0 && val > whiteVal) {
            whiteVal = val;
            whiteIdx = i;
        }
    }
    whiteIdx -= 10;
    blackIdx += 10;
    // scale image range (between black and white) to full range
    double mul = 255.0 / (whiteIdx - blackIdx);
    double add = - blackIdx * mul;
    cv::Mat ocrImage;
    cv::convertScaleAbs(grayImage, ocrImage, mul, add);
    return ocrImage;
}

int ocrDetectorText(TextType tt, const cv::Mat& grayImage, const ResolvedEnv&, std::string& text, cv::Rect* rectOut) {
    double scale = ocr::LINE_HEIGHT / double(grayImage.rows);
    cv::Mat scaledImage = scaleImage(const_cast<cv::Mat&>(grayImage), scale, true);
    cv::bitwise_not(scaledImage, scaledImage);
    cv::Mat ocrImage = normalizeDetectorText(scaledImage);
    int conf = ocr::ocrLine(tt, "(detector text)", ocrImage, text, rectOut);
    if (scale != 1 && rectOut) {
        rectOut->x = (int)(std::round(rectOut->x / scale));
        rectOut->y = (int)(std::round(rectOut->y / scale));
        rectOut->width = (int)(std::round(rectOut->width / scale));
        rectOut->height = (int)(std::round(rectOut->height / scale));
    }
    return conf;
}

}
