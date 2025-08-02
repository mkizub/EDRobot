//
// Created by mkizub on 22.07.2025.
//

#include "pch.h"

#include "OCR.h"
#include "EDWidget.h"

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include <opencv2/dnn_superres.hpp>

namespace ocr {

static std::mutex tesseractMutex;

static tesseract::TessBaseAPI* tesseractApi;
//static tesseract::TessBaseAPI* tesseractApiTmp;
//static cv::dnn_superres::DnnSuperResImpl dnnSuperRes;

void init(const std::string& tessdata, Lang lng) {
    LOG(INFO) << "Initializing Tesseract OCR for lang '" << enum_name<Lang>(lng) << "', tessdata: " << tessdata << "";
    const char* tesseractLang = "edr";
    if (lng == RU) {
        tesseractLang = "edr";
    }
    tesseractApi = new tesseract::TessBaseAPI();
    int fail = tesseractApi->Init(tessdata.c_str(), tesseractLang, tesseract::OEM_DEFAULT, nullptr, 0, 0, 0, true);
    if (fail) {
        LOG(ERROR) << "Error: Could not initialize tesseract.";
        shutdown();
    } else {
        tesseractApi->SetPageSegMode(tesseract::PSM_SINGLE_LINE); // PSM_RAW_LINE
    }

//    tesseractApiTmp = new tesseract::TessBaseAPI();
//    fail = tesseractApiTmp->Init(tessdata.c_str(), "edr-s", tesseract::OEM_DEFAULT, nullptr, 0, 0, 0, true);
//    if (fail) {
//        LOG(ERROR) << "Error: Could not initialize tesseract.";
//        shutdown();
//    } else {
//        tesseractApiTmp->SetPageSegMode(tesseract::PSM_SINGLE_LINE); // PSM_RAW_LINE
//    }
//
//    dnnSuperRes.readModel("models/EDSR_x2.pb");
//    dnnSuperRes.setModel("edsr", 2);
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

int ocrLine(const char* dbg, const cv::Mat& grayImage, std::string& text, cv::Rect* rect) {
    text.clear();
    if (rect)
        *rect = {};
    std::scoped_lock<std::mutex> lock(tesseractMutex);
    if (!tesseractApi)
        return 0;

    // 'edr'
    auto startTime = std::chrono::high_resolution_clock::now();

    tesseractApi->SetImage(grayImage.data, grayImage.cols, grayImage.rows, 1, (int)grayImage.step);
    tesseractApi->Recognize(nullptr);
    int conf = tesseractApi->MeanTextConf();
    if (conf != 0) {
        const char *outText = tesseractApi->GetUTF8Text();
        text = trim(outText);
        delete[] outText;
        if (rect) {
            int left, top, right, bottom;
            tesseract::ResultIterator* ri = tesseractApi->GetIterator();
            if (ri->BoundingBox(tesseract::PageIteratorLevel::RIL_TEXTLINE, 1, &left, &top, &right, &bottom))
                *rect = {left, top, right - left, bottom - top};
            delete ri;
        }
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

const int ADD_TOP = 0;
const int ADD_BOTTOM = 0;
int ocrRowText(const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, int tab, std::string& text) {
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
    cv::Mat rowImage(grayImage, capturedRect);
    cv::Mat scaledImage = scaleImage(rowImage, scale, cr.u.lrow.ws != WState::Focused);
    if (cr.u.lrow.ws != WState::Focused)
        cv::bitwise_not(scaledImage, scaledImage);
    int ocr_top = t.ocr_top * scale - ocr::LEADING;
    cv::Rect ocrRect {0, ocr_top-ADD_TOP, scaledImage.cols, ocr::LINE_HEIGHT+ADD_TOP+ADD_BOTTOM};
    ocrRect &= cv::Rect(0, 0, scaledImage.cols, scaledImage.rows);
    cv::Mat ocrImage(scaledImage, ocrRect);
    int conf = ocr::ocrLine("(list row)", ocrImage, text, nullptr);
    return conf;
}

int ocrRowTextForTraining(const cv::Mat& grayImage, const ResolvedEnv& rEnv, const ClassifiedRect& cr, int tab, std::string& text, cv::Mat& dumpImage) {
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
    cv::Rect ocrRect {0, ocr_top, scaledImage.cols, ocr::LINE_HEIGHT};
    assert (ocr_top >= 0 && ocr_top+ocr::LINE_HEIGHT < scaledImage.rows);
    ocrRect &= cv::Rect(0, 0, scaledImage.cols, scaledImage.rows);
    cv::Mat ocrImage(scaledImage, ocrRect);
    cv::Rect rect;
    int conf = ocr::ocrLine("(list row training)", ocrImage, text, &rect);
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
        cv::Rect ocrLineRect (0, l*ocr::LINE_HEIGHT-ADD_TOP, scaledImage.cols, ocr::LINE_HEIGHT+ADD_TOP+ADD_BOTTOM);
        cv::Mat ocrImage(scaledImage, ocrLineRect);
        std::string line;
        int conf = ocr::ocrLine("(market lbl)", ocrImage, line, nullptr);
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
        int conf = ocr::ocrLine("(market lbl training)", ocrImage, text, &rect);
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
    lblCapturedRect.y -= ADD_TOP;
    lblCapturedRect.height += ADD_TOP+ADD_BOTTOM;
    cv::Mat lblImage(grayImage, lblCapturedRect);

    cv::Mat scaledImage = scaleImage(lblImage, scale, true);
    cv::bitwise_not(scaledImage, scaledImage);

    int conf = ocr::ocrLine("(nav lbl)", scaledImage, text, nullptr);
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
    int conf = ocr::ocrLine("(nav lbl training)", scaledImage, text, &rect);
    if (conf < 50) {
        dumpImage = scaledImage;
    } else {
        dumpImage = cv::Mat(scaledImage, {rect.x, 0, rect.width, scaledImage.rows});
    }

    return conf;
}
}
