//
// Created by mkizub on 22.07.2025.
//

#include "pch.h"

#include "OCR.h"
#include "widget/EDWidget.h"
#include "widget/List.h"
#include "FuzzyMatch.h"

#include <tesseract/baseapi.h>
#include <tesseract/capi.h>
#include <leptonica/allheaders.h>

namespace ocr {

static std::mutex tesseractMutex;

static tesseract::TessBaseAPI* tesseractApi;
//static tesseract::TessBaseAPI* tesseractApiTmp;
//static cv::dnn_superres::DnnSuperResImpl dnnSuperRes;

//#define DEBUG_OCR 1
#if defined(DEBUG_OCR) && defined(NDEBUG)
# error "DEBUG_OCR in release build"
#endif


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
    std::vector<std::string> vars_vec { "user_words_file", "debug_file", "tessedit_do_invert"};
    std::vector<std::string> vars_values { "cache/tesseract-words.txt", "cache/tesseract.log", "0" };
    tesseractApi = new tesseract::TessBaseAPI();
    int fail = tesseractApi->Init(tessdata.c_str(), tesseractLang, tesseract::OEM_DEFAULT, nullptr, 0,
                                  &vars_vec, &vars_values, true);
    if (fail) {
        LOG(ERROR) << "Error: Could not initialize tesseract.";
        shutdown();
        return false;
    }
    tesseractApi->SetVariable("user_words_file", "cache/tesseract-words.txt");
    tesseractApi->SetVariable("debug_file", "cache/tesseract.log");
    tesseractApi->SetVariable("tessedit_do_invert", "0");
    tesseractApi->SetPageSegMode(tesseract::PSM_SINGLE_LINE); // PSM_RAW_LINE
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

bool ocrPageSegm(const cv::Mat& grayImage, cv::Rect& rectOut, std::vector<cv::Line>& baselineOut) {
    rectOut = {};
    baselineOut.clear();

    if (!tesseractApi || grayImage.empty())
        return false;
    assert (grayImage.type() == CV_8UC1);

    std::scoped_lock<std::mutex> lock(tesseractMutex);

    tesseractApi->SetVariable("tessedit_char_whitelist", "");
    tesseractApi->SetPageSegMode(tesseract::PSM_AUTO_ONLY);
    tesseractApi->SetImage(grayImage.data, grayImage.cols, grayImage.rows, 1, (int)grayImage.step);
    tesseractApi->Recognize(nullptr);
    tesseract::ResultIterator* ri = tesseractApi->GetIterator();
    if (!ri)
        return false;
    ri->Begin();
    int x1, y1, x2, y2;
    if (ri->BoundingBox(tesseract::PageIteratorLevel::RIL_PARA, 1, &x1, &y1, &x2, &y2)) {
        cv::Rect bb = {x1, y1, x2 - x1, y2 - y1};
        rectOut = bb;
    }
    do {
        if (ri->Baseline(tesseract::PageIteratorLevel::RIL_TEXTLINE, &x1, &y1, &x2, &y2)) {
            baselineOut.emplace_back(x1, y1, x2, y2);
            int y_min = std::min(y1, y2)-1;
            int y_max = std::min(y1, y2)+1;
            rectOut |= cv::Rect(cv::Point(x1,y_min), cv::Point(x2, y_max));
        }
    } while (ri->Next(tesseract::PageIteratorLevel::RIL_TEXTLINE));

    return !baselineOut.empty();
}


int ocrLine(TextType type, int psm, const char* dbg, const cv::Mat& grayImage, int minConf, std::string& text, cv::Rect* rectOut) {
    text.clear();
    if (rectOut)
        *rectOut = {};
    if (!tesseractApi || grayImage.empty())
        return 0;
    //assert (grayImage.rows == ocr::LINE_HEIGHT);
    assert (grayImage.type() == CV_8UC1);

    std::scoped_lock<std::mutex> lock(tesseractMutex);
    auto startTime = std::chrono::high_resolution_clock::now();

    tesseractApi->SetPageSegMode((tesseract::PageSegMode)psm);
    switch (type) {
    case GENERIC:
        tesseractApi->SetVariable("tessedit_char_whitelist", "");
        break;
    case DISTANCE:
        if (st::lng == Lang::RU)
            tesseractApi->SetVariable("tessedit_char_whitelist", " .,/%0123456789Mмкcвл");
        else
            tesseractApi->SetVariable("tessedit_char_whitelist", " .,/%0123456789Mmklsy");
        break;
    case NUMERIC:
        tesseractApi->SetVariable("tessedit_char_whitelist", " +-.,/%0123456789");
        break;
    }

    tesseractApi->SetImage(grayImage.data, grayImage.cols, grayImage.rows, 1, (int)grayImage.step);
    tesseractApi->Recognize(nullptr);
    int conf = tesseractApi->MeanTextConf();
    if (conf >= minConf) {
#ifdef DEBUG_OCR
        {
            char *word{};
            word = tesseractApi->GetUTF8Text();
            std::wstring whole = toUtf16(word);
            TessDeleteText(word);
        }
#endif
        bool valid = true;
        tesseract::ResultIterator* ri = tesseractApi->GetIterator();
        cv::Rect lr;
        conf = 0;
        int count = 0;
        ri->Begin();
        do {
            char *word = ri->GetUTF8Text(tesseract::PageIteratorLevel::RIL_WORD);
            if (!word || !*word)
                continue;
            int left, top, right, bottom;
            ri->BoundingBox(tesseract::PageIteratorLevel::RIL_TEXTLINE, 1, &left, &top, &right, &bottom);
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
    }
    tesseractApi->Clear();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    LOG(DEBUG) << "OCR Output ("<<dbg<<"): '" << text << "' words conf=" << conf << "% took=" << elapsedTime.count() << "us";

    return conf;
}

static cv::Mat scaleImage(const cv::Mat& image, double scale) {
    if (scale == 1) {
        return image.clone();
    }
    /*if (scale < 1 || image.channels() != 3)*/ {
        cv::Mat scaledImage;
        cv::resize(image, scaledImage, {0,0}, scale, scale, cv::INTER_CUBIC);
        return scaledImage;
    }
}

cv::Mat normalizeTextImage(cv::Mat& grayImage, const int blackPixelsLimit, const int blackAdd, const int whiteSub, const int bin) {
    const int histSize = 256/bin;
    float range[]{0, 256}; //the upper boundary is exclusive
    const float *histRange[]{range};
    cv::Mat hist;
    cv::calcHist(&grayImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);

#ifdef DEBUG_OCR
    // Normalize histogram
    std::vector<float> hn;
    cv::normalize(hist, hn, 0.0, 1.0, cv::NORM_MINMAX, -1, cv::Mat());
    // Create histogram image
    int hist_w = 256;
    int hist_h = 200;
    cv::Mat histImage(hist_h, hist_w, CV_8UC3, cv::Scalar(0, 0, 0));
    // Draw lines for each bin
    for (int i = 1; i < histSize; i++) {
        cv::line(histImage,
                 cv::Point(bin * (i - 1), cvRound(hist_h*(1.0-hn[i - 1]))),
                 cv::Point(bin * (i), cvRound(hist_h*(1.0-hn[i]))),
                 cv::Scalar(255, 255, 255), 2, 8, 0); // White lines for grayscale
    }
#endif

    int blackIdx=-1, whiteIdx=-1;
    float blackCnt=0, whiteMax=0;
    auto* hd = (float*)hist.data;
    // black requires min count, white is the max of histogram
    for (int i=0; i < histSize; i++) {
        float val = hd[i];
        if (val <= 0)
            continue;
        if (blackIdx < 0) {
            blackCnt += val;
            if (blackCnt >= blackPixelsLimit)
                blackIdx = i*bin + bin/2;
        }
        else if (val > whiteMax) {
            whiteMax = val;
            whiteIdx = i*bin + bin/2;
        }
    }
    if (whiteIdx - blackIdx <= blackAdd+whiteSub+10)
        return {};
    blackIdx += blackAdd;
    whiteIdx -= whiteSub;
    // scale image range (between black and white) to full range
    double mul = 255.0 / (whiteIdx - blackIdx);
    double add = - blackIdx * mul;
    //cv::Mat ocrImage;
    //cv::convertScaleAbs(grayImage, ocrImage, mul, add);
    //return ocrImage;
    cv::addWeighted(grayImage, mul, grayImage, 0, add, grayImage);
    return grayImage;
}


int ocrRowText(TextType tt, const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, std::string_view tab_name, std::string& text, cv::Rect* rectOut) {
    assert (cr.cdt == ClsDetType::ListRow);
    const widget::List* lst = cr.u.lrow.list;
    auto& t = lst->getTab(tab_name);
    if (t.tab_right <= t.tab_left || t.ocr_height <= 0)
        return 0;
    double scale = (ocr::ASCENT+ocr::DESCENT) / t.ocr_height / rEnv.getScale();
    cv::Rect capturedRect = cr.u.lrow.capturedRect;
    capturedRect.x += int(t.tab_left * rEnv.getScale());
    capturedRect.y += 2;
    capturedRect.width = int((t.tab_right - t.tab_left) * rEnv.getScale());
    capturedRect.height -= 4;
    capturedRect &= cv::Rect(0,0,grayImage.cols,grayImage.rows);
    cv::Mat rowImage(grayImage, capturedRect);
    cv::Mat scaledImage = scaleImage(rowImage, scale);
    if (cr.u.lrow.ws != WState::Focused)
        cv::bitwise_not(scaledImage, scaledImage);
    cv::Mat ocrImage = normalizeTextImage(scaledImage);
    cv::Rect rectPSM;
    std::vector<cv::Line> baselines;
    if (!ocr::ocrPageSegm(ocrImage, rectPSM, baselines))
        return 0;
    if (rectOut) {
        rectOut->x = (int)(std::round(rectPSM.x / scale));
        rectOut->y = (int)(std::round(rectPSM.y / scale));
        rectOut->width = (int)(std::round(rectPSM.width / scale));
        rectOut->height = (int)(std::round(rectPSM.height / scale));
    }
#ifdef DEBUG_OCR
    cv::Mat debugImage = ocrImage.clone();
    cv::rectangle(debugImage, rectPSM, {64,64,64}, 1);
    for (auto& bl : baselines)
        cv::line(debugImage, bl.p0(), bl.p1(), {64,64,64}, 1);
#endif

    int conf = 0;
    for (auto& bl : baselines) {
        cv::Rect crop;
        crop.x = rectPSM.x;
        crop.width = rectPSM.width;
        crop.y = bl.y0 - (ocr::ASCENT + ocr::LEADING);
        if (crop.y < 0)
            crop.y = 0;
        else if (crop.y + ocr::LINE_HEIGHT > ocrImage.rows)
            crop.y = ocrImage.rows - ocr::LINE_HEIGHT;
        crop.height = ocr::LINE_HEIGHT;
#ifdef DEBUG_OCR
        cv::rectangle(debugImage, crop, {64,64,64}, 1);
#endif
        crop &= cv::Rect(0, 0, ocrImage.cols, ocrImage.rows);
        cv::Mat ocrCropImage = ocrImage(crop);
        std::string text_line;
        conf += ocr::ocrLine(tt, 13, "(row text)", ocrCropImage, 30, text_line, nullptr);
        if (!text.empty())
            text += " ";
        text += text_line;
        break;
    }
    //if (baselines.size() > 1)
    //    conf /= baselines.size();
    return conf;
}

int ocrRowTextForTraining(TextType tt, const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, std::string_view tab_name, std::string& text, cv::Mat& dumpImage) {
    assert (cr.cdt == ClsDetType::ListRow);
    const widget::List* lst = cr.u.lrow.list;
    auto& t = lst->getTab(tab_name);
    if (t.tab_right <= t.tab_left || t.ocr_height <= 0)
        return 0;
    double scale = (ocr::ASCENT+ocr::DESCENT) / t.ocr_height / rEnv.getScale();
    cv::Rect capturedRect = cr.u.lrow.capturedRect;
    capturedRect.x += int(t.tab_left * rEnv.getScale());
    capturedRect.y += 2;
    capturedRect.width = int((t.tab_right - t.tab_left) * rEnv.getScale());
    capturedRect.height -= 4;
    capturedRect &= cv::Rect(0,0,grayImage.cols,grayImage.rows);
    cv::Mat rowImage(grayImage, capturedRect);
    cv::Mat scaledImage = scaleImage(rowImage, scale);
    if (cr.u.lrow.ws != WState::Focused)
        cv::bitwise_not(scaledImage, scaledImage);
    cv::Mat ocrImage = normalizeTextImage(scaledImage);
    cv::Rect rectPSM;
    std::vector<cv::Line> baselines;
    if (!ocr::ocrPageSegm(ocrImage, rectPSM, baselines)) {
        dumpImage = ocrImage;
        return 0;
    }
#ifdef DEBUG_OCR
    cv::Mat debugImage = ocrImage.clone();
    cv::rectangle(debugImage, rectPSM, {64,64,64}, 1);
    for (auto& bl : baselines)
        cv::line(debugImage, bl.p0(), bl.p1(), {64,64,64}, 1);
#endif

    auto& bl = baselines[0];
    cv::Rect crop;
    crop.x = rectPSM.x;
    crop.width = rectPSM.width;
    crop.y = bl.y0 - (ocr::ASCENT + ocr::LEADING);
    if (crop.y < 0)
        crop.y = 0;
    else if (crop.y + ocr::LINE_HEIGHT > ocrImage.rows)
        crop.y = ocrImage.rows - ocr::LINE_HEIGHT;
    crop.height = ocr::LINE_HEIGHT;
#ifdef DEBUG_OCR
    cv::rectangle(debugImage, crop, {64,64,64}, 1);
#endif
    cv::Mat cropImage = ocrImage(crop);
    cv::Rect rect13;
    int conf = ocr::ocrLine(tt, 13, "(row text)", cropImage, 0, text, &rect13);
    dumpImage = ocrImage(crop);
    return conf;
}

int ocrMarketLblText(const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, std::string& text) {
    assert (cr.cdt == ClsDetType::Widget);
    assert (cr.u.widg.widget->tp == widget::WidgetType::Label);
    const widget::Label* lbl = (const widget::Label*)cr.u.widg.widget;
    double scale = (ocr::ASCENT+ocr::DESCENT) / lbl->mFontHeight / rEnv.getScale();

    cv::Rect lblCapturedRect = rEnv.cvtReferenceToCaptured(cr.detectedRect);
    cv::Mat lblImage(grayImage, lblCapturedRect);

    cv::Mat scaledImage = scaleImage(lblImage, scale);
    cv::bitwise_not(scaledImage, scaledImage);
    cv::Mat ocrImage = normalizeTextImage(scaledImage);

    cv::Rect rect;
    std::vector<cv::Line> baselines;
    if (!ocr::ocrPageSegm(ocrImage, rect, baselines))
        return 0;

    int conf = 0;
    for (auto& bl : baselines) {
        cv::Rect crop;
        crop.x = rect.x;
        crop.y = bl.y0 - (ocr::ASCENT + ocr::LEADING);
        crop.width = rect.width;
        crop.height = ocr::LINE_HEIGHT;
        crop &= cv::Rect(0, 0, ocrImage.cols, ocrImage.rows);
        cv::Mat ocrCropImage = ocrImage(crop);
        std::string text_line;
        conf += ocr::ocrLine(ocr::GENERIC, 13, "(market lbl)", ocrCropImage, 30, text_line, nullptr);
        if (!text.empty())
            text += " ";
        text += text_line;
    }
    conf /= baselines.size();
    return conf;
}

int ocrMarketLblTextForTraining(const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, std::vector<std::string>& texts, std::vector<cv::Mat>& dumpImages) {
    assert (cr.cdt == ClsDetType::Widget);
    assert (cr.u.widg.widget->tp == widget::WidgetType::Label);
    const widget::Label* lbl = (const widget::Label*)cr.u.widg.widget;
    double scale = (ocr::ASCENT+ocr::DESCENT) / lbl->mFontHeight / rEnv.getScale();

    cv::Rect lblCapturedRect = rEnv.cvtReferenceToCaptured(cr.detectedRect);
    cv::Mat lblImage(grayImage, lblCapturedRect);

    cv::Mat scaledImage = scaleImage(lblImage, scale);
    cv::bitwise_not(scaledImage, scaledImage);

    int lines = (int)std::round(double(scaledImage.rows) / double(ocr::LINE_HEIGHT));
    int ocr_conf_sum = 0;
    for (int l=0; l < lines; l++) {
        cv::Rect ocrLineRect (0, l*ocr::LINE_HEIGHT, scaledImage.cols, ocr::LINE_HEIGHT);
        cv::Mat ocrImage(scaledImage, ocrLineRect);
        std::string text;
        cv::Rect rect;
        int conf = ocr::ocrLine(TextType::GENERIC, 7, "(market lbl training)", ocrImage, 30, text, &rect);
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
    double scale = (ocr::ASCENT+ocr::DESCENT) / lbl->mFontHeight / rEnv.getScale();

    cv::Rect lblCapturedRect = rEnv.cvtReferenceToCaptured(cr.detectedRect);
    cv::Mat lblImage(grayImage, lblCapturedRect);

    cv::Mat scaledImage = scaleImage(lblImage, scale);
    cv::bitwise_not(scaledImage, scaledImage);

    int conf = ocr::ocrLine(TextType::GENERIC, 7, "(nav lbl)", scaledImage, 30, text, nullptr);
    return conf;
}

int ocrNavigationLblTextForTraining(const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, std::string& text, cv::Mat& dumpImage) {
    assert (cr.cdt == ClsDetType::Widget);
    assert (cr.u.widg.widget->tp == widget::WidgetType::Label);
    const widget::Label* lbl = (const widget::Label*)cr.u.widg.widget;
    double scale = (ocr::ASCENT+ocr::DESCENT) / lbl->mFontHeight / rEnv.getScale();

    cv::Rect lblCapturedRect = rEnv.cvtReferenceToCaptured(cr.detectedRect);
    cv::Mat lblImage(grayImage, lblCapturedRect);

    cv::Mat scaledImage = scaleImage(lblImage, scale);
    cv::bitwise_not(scaledImage, scaledImage);

    cv::Rect rect;
    int conf = ocr::ocrLine(TextType::GENERIC, 7, "(nav lbl training)", scaledImage, 30, text, &rect);
    if (conf < 50) {
        dumpImage = scaledImage;
    } else {
        dumpImage = cv::Mat(scaledImage, {rect.x, 0, rect.width, scaledImage.rows});
    }

    return conf;
}

cv::Mat normalizeTargetDistText(cv::Mat& grayImage) {
    int histSize = 256;
    float range[]{0, 256}; //the upper boundary is exclusive
    const float *histRange[]{range};
    cv::Mat hist;
    cv::calcHist(&grayImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
    cv::GaussianBlur(hist, hist, cv::Size(9,9), 0); // TODO: maybe not needed

#ifdef DEBUG_OCR
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
#endif

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
    //cv::Mat ocrImage;
    //cv::convertScaleAbs(grayImage, ocrImage, mul, add);
    //return ocrImage;
    cv::addWeighted(grayImage, mul, grayImage, 0, add, grayImage);
    return grayImage;
}

int ocrTargetDistText(cv::Mat grayImage, std::string& text) {
    cv::Mat ocrImage = normalizeTargetDistText(grayImage);
    int conf = ocr::ocrLine(ocr::DISTANCE, 7, "(nav dist)", ocrImage, 30, text, nullptr);
    return conf;
}

int ocrTileLblText(double font_height, cv::Mat& grayImage, WState ws, std::string& text) {
    double scale = (ocr::ASCENT+ocr::DESCENT) / font_height;
    cv::Mat scaledImage = scaleImage(const_cast<cv::Mat&>(grayImage), scale);
    if (ws != WState::Focused)
        cv::bitwise_not(scaledImage, scaledImage);
    cv::Mat ocrImage = normalizeTextImage(scaledImage);

    cv::Rect rect;
    std::vector<cv::Line> baselines;
    if (!ocr::ocrPageSegm(ocrImage, rect, baselines))
        return 0;
#ifdef DEBUG_OCR
    cv::Mat debugImage = ocrImage.clone();
    cv::rectangle(debugImage, rect, {64,64,64}, 1);
    for (auto& bl : baselines)
        cv::line(debugImage, bl.p0(), bl.p1(), {64,64,64}, 1);
#endif

    int conf = 0;
    for (auto& bl : baselines) {
        cv::Rect crop;
        crop.x = rect.x;
        crop.y = bl.y0 - (ocr::ASCENT + ocr::LEADING);
        crop.width = rect.width;
        crop.height = ocr::LINE_HEIGHT;
        crop &= cv::Rect(0, 0, ocrImage.cols, ocrImage.rows);
        cv::Mat ocrCropImage = ocrImage(crop);
        std::string text_line;
        conf += ocr::ocrLine(ocr::GENERIC, 13, "(tile text)", ocrCropImage, 30, text_line, nullptr);
        if (!text.empty())
            text += " ";
        text += text_line;
    }
    conf /= baselines.size();
    return conf;
}

int ocrDetectorText(TextType tt, double font_height, bool multiline, const cv::Mat& grayImage, const ResolvedEnv&, std::string& text, cv::Rect* rectOut) {
    double scale = (ocr::ASCENT+ocr::DESCENT) / font_height;
    cv::Mat scaledImage = scaleImage(const_cast<cv::Mat&>(grayImage), scale);
    cv::bitwise_not(scaledImage, scaledImage);
    cv::Mat ocrImage = normalizeTextImage(scaledImage, 50, 0, 10, 4);

    cv::Rect rect;
    std::vector<cv::Line> baselines;
    if (!ocr::ocrPageSegm(ocrImage, rect, baselines))
        return 0;
#ifdef DEBUG_OCR
    cv::Mat debugImage = ocrImage.clone();
    cv::rectangle(debugImage, rect, {64,64,64}, 1);
    for (auto& bl : baselines)
        cv::line(debugImage, bl.p0(), bl.p1(), {64,64,64}, 1);
#endif
    if (rectOut) {
        rectOut->x = (int)(std::round(rect.x / scale));
        rectOut->y = (int)(std::round(rect.y / scale));
        rectOut->width = (int)(std::round(rect.width / scale));
        rectOut->height = (int)(std::round(rect.height / scale));
    }

    int conf = 0;
    for (auto& bl : baselines) {
        cv::Rect crop;
        crop.x = rect.x;
        crop.y = bl.y0 - (ocr::ASCENT + ocr::LEADING);
        crop.width = rect.width;
        crop.height = ocr::LINE_HEIGHT;
        crop &= cv::Rect(0, 0, ocrImage.cols, ocrImage.rows);
        cv::Mat ocrCropImage = ocrImage(crop);
        std::string text_line;
        int c = ocr::ocrLine(ocr::GENERIC, 13, "(detector text)", ocrCropImage, 30, text_line, nullptr);
        if (multiline) {
            conf += c;
            if (!text.empty())
                text += "\n";
            text += text_line;
        }
        else if (c > conf) {
            conf = c;
            text = text_line;
        }
    }
    if (multiline)
        conf /= baselines.size();
    return conf;
}

}
