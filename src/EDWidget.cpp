//
// Created by mkizub on 31.05.2025.
//

// conflicts with _() of gettext, have to include before pch.h
#include <peglib/peglib.h>

#include "pch.h"

#include "EDWidget.h"
#include "OCR.h"
#include "State.h"
#include "detect/Detector.h"

#ifndef NDEBUG
//#include <cpptrace/cpptrace.hpp>
#include "cpptrace/from_current.hpp"
#include "Master.h"

#endif

#ifdef CPPTRACE_TRY
# define TRY CPPTRACE_TRY
# define CATCH(param) CPPTRACE_CATCH(param)
# define GET_EXCEPTION_STACK_TRACE cpptrace::from_current_exception().to_string()
#else
# define TRY try
# define CATCH(param) catch(param)
# include <stacktrace>
# define GET_EXCEPTION_STACK_TRACE std::stacktrace::current()
#endif


namespace widget {

typedef std::shared_ptr<peg::Ast> spAst;

void Widget::addSubItem(Widget *sub) {
    if (!sub)
        return;
    if (!sub->parent)
        sub->parent = this;
    have.push_back(sub);
}

Widget::~Widget() {
    oracle.reset();
}

Widget::Widget(WidgetType tp, const std::string &name, Widget *parent)
    : tp(tp)
    , name(name)
    , parent(nullptr)
    , path((parent && parent->tp != WidgetType::Root) ? parent->path + ":" + name : name)
    , oracle(nullptr)
{
    //if (parent)
    //    parent->addSubItem(this);
}

void Widget::setRect(const char* name, const json5pp::value& value) {
    rect = makeEvalRect(*this, name, value[name]);
}

cv::Rect Widget::calcReferenceRect(const ClassifyEnv& env) const {
    if (!rect)
        return {};
    return rect->calcReferenceRect(env);
}

static bool safeDetect(Widget* widget, Widget::DetectParams& params) {
    if (!widget)
        return false;
    bool detected = false;
    TRY {
        detected = widget->detect(params);
    } CATCH(const std::exception& e) {
        LOG(ERROR) << "Exception in widget '" << widget->path << "' detection: " << e.what() << std::endl << GET_EXCEPTION_STACK_TRACE;
#ifndef NDEBUG
        throw;
#endif
    }
    return detected;
}

bool Root::detect(DetectParams& params) {
    if (params.level < DetectLevel::Screen)
        return true;
    for (auto widget: this->have) {
        if (!widget || widget->tp != WidgetType::Screen)
            continue;
        if (safeDetect(widget, params))
            return true;
    }
    return false;
}

bool Screen::detect(DetectParams& params) {
    if (!this->checkStatus())
        return false;

    if (oracle) {
        double match = oracle->match(params.env);
        if (match < 0.5)
            return false;
    }

    if (transform) {
        params.env.warpPerspective(transform);
        if (transform->valid)
            params.env.setWarpMode(transform->valid);
    }

    bool modeMatch = true;
    for (auto mode: this->have) {
        if (!mode || !(mode->tp == WidgetType::Mode || mode->tp == WidgetType::Dialog))
            continue;
        modeMatch = safeDetect(mode, params);
        if (modeMatch)
            break;
    }
    if (!modeMatch) {
        params.env.setWarpMode(false);
        return false;
    }
    if (!params.uiState.screen)
        params.uiState.screen = this;
    if (!params.uiState.widget)
        params.uiState.widget = this;

    if (params.level <= DetectLevel::Screen) {
        params.env.setWarpMode(false);
        return true;
    }

    for (auto widget: this->have) {
        if (!widget || widget->tp == WidgetType::Mode || widget->tp == WidgetType::Dialog)
            continue;
        safeDetect(widget, params);
    }

    params.env.setWarpMode(false);
    return true;
}

bool Dialog::detect(DetectParams& params) {
    if (oracle) {
        double match = oracle->match(params.env);
        if (match < 0.5)
            return false;
        if (!params.uiState.widget || params.uiState.widget == parent)
            params.uiState.widget = this;
    }

    bool modeMatch = true;
    for (auto mode: this->have) {
        if (!mode || mode->tp != WidgetType::Mode)
            continue;
        modeMatch = safeDetect(mode, params);
        if (modeMatch)
            break;
    }
    if (!modeMatch)
        return false;

    for (auto widget: this->have) {
        if (!widget || widget->tp == WidgetType::Mode)
            continue;
        safeDetect(widget, params);
    }

    return true;
}

bool Mode::detect(DetectParams& params) {
    if (!oracle)
        return false;
    double match = oracle->match(params.env);
    if (match < 0.5)
        return false;
    if (!params.uiState.widget || params.uiState.widget == parent || params.uiState.widget == parent->parent)
        params.uiState.widget = this;

    for (auto widget: this->have) {
        safeDetect(widget, params);
    }

    return true;
}

bool Label::detect(DetectParams& params) {
    cv::Rect r = params.env.calcReferenceRect(this->rect);
    if (ocr_bot > 0) {
        int reference_line_height = (int) std::round(ocr::LINE_HEIGHT * double(ocr_bot - ocr_top) / double(ocr::ASCENT+ocr::DESCENT));
        int lines = (int) std::round(double(r.height) / double(reference_line_height));
        r.height = lines * reference_line_height;
    }
    params.env.classified.emplace_back(ClsDetType::Widget, params.env.isWarpMode(), this->name, r);
    ClassifiedRect& clsLblRect = params.env.classified.back();
    clsLblRect.u.widg.referenceRect = r;
    clsLblRect.u.widg.ws = WState::Unknown;
    clsLblRect.u.widg.widget = this;
    return true;
}

bool BaseButton::detect(DetectParams& params) {
    ClassifyEnv& env = params.env;
    cv::Rect expectedR = env.calcReferenceRect(this->rect);
    if (expectedR.empty())
        return false;
    cv::Rect detectedR = expectedR;
    cv::Rect captureR = env.cvtCapturedToReference(expectedR);
    if (captureR.empty())
        return false;

    if (extendLT != cv::Point() || extendRB != cv::Point()) {
        cv::Rect extendR = {expectedR.tl() - extendLT, expectedR.br() + extendRB};
        cv::Rect matchR = env.cvtReferenceToCaptured(extendR);

        cv::Vec3b hsvColorMin {5, 127, 30};
        cv::Vec3b hsvColorMax {30, 255, 255};
        XMat hsvImage;
        cv::cvtColor(env.getColorImage()(matchR), hsvImage, cv::COLOR_BGR2HSV);
        XMat thrImage;
        cv::inRange(hsvImage, hsvColorMin, hsvColorMax, thrImage);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContoursLinkRuns(thrImage, contours);
        for (auto &cont: contours) {
            std::vector<cv::Point> convex;
            cv::convexHull(cont, convex);
            if (convex.size() >= 4) {
                std::vector<cv::Point> approx;
                cv::approxPolyN(convex, approx, 4, 5, true);
                cv::Rect bbox = cv::boundingRect(approx);
                bbox &= cv::Rect(cv::Point(),matchR.size());
                if (bbox.width > captureR.width*0.9 && bbox.height > captureR.height*0.9 &&
                    bbox.width < captureR.width*1.1 && bbox.height < captureR.height*1.2)
                {
                    captureR = {matchR.tl() + bbox.tl(), bbox.size()};
                    detectedR = env.cvtCapturedToReference(captureR);
                    break;
                }
            }
        }
    }

    env.classified.emplace_back(ClsDetType::Widget, env.isWarpMode(), this->name, detectedR);
    ClassifiedRect& clsBtnRect = env.classified.back();
    clsBtnRect.u.widg.referenceRect = expectedR;
    clsBtnRect.u.widg.ws = WState::Unknown;
    clsBtnRect.u.widg.widget = this;

    detect::Histogram histDet(detect::Histogram::Mode::Hsv, detectedR);
    if (!histDet.calc(env))
        return false;
    WState ws = WState::Unknown;
    if (histDet.mLastColor[2] > 10) { // not black
        if (histDet.mLastColor[1] < 80) // desaturated = disabled
            ws = WState::Disabled;
        else if (histDet.mLastColor[0] < 30) {// hue is near red = known color
            if (histDet.mLastColor[2] > 180) // bright = focused
                ws = WState::Focused;
            else
                ws = WState::Normal;
        }
    }
    clsBtnRect.u.widg.ws = ws;
    LOG_IF(ws == WState::Focused, INFO) << "Focused: " << this->path;
    LOG_IF(ws == WState::Disabled, INFO) << "Disabld: " << this->path;
    if (ws == WState::Focused && !params.uiState.focused)
        params.uiState.focused = this;

    return true;
}

struct Pixel {
    enum State { UNKNOWN, FLAT, GAP, ROW, EMPTY };
    short real;
    short base;
    short range_id;
    bool artificial;
    State state;
    Pixel(uchar ch) : real(ch), base(ch), range_id(0), state(UNKNOWN), artificial(false) {}
};

struct Redused {
    static Pixel empty;
    const List* list;
    const int size;
    const double scale;
    std::vector<Pixel> pix;
    int range_count;
    int empty_threshold;
    Redused(List* list, uchar* reduced, int len, double scale)
        : list(list)
        , size(len)
        , scale(scale)
        , range_count(0)
        , empty_threshold(100)
    {
        pix.reserve(len);
        for (int i=0; i < len; i++)
            pix.emplace_back(reduced[i]);
        if (list->header > 0)
            empty_threshold = 70;
    }
    const Pixel& at(int i) {
        if (i < 0 || i >= size)
            return empty;
        return pix[i];
    }
    int get_range_end(int start);
    bool get_flat_range(int start, int& end);
    int maybe_gap_range(int start, int min_gap, int max_gap);
    int maybe_row_range(int start, int min_row, int max_row);
    void detect_flat();
    void clear_empty();
    void detect_gaps();
    void detect_rows();
    bool insert_gaps();
    bool insert_rows();
    bool normalize_rows();
};

Pixel Redused::empty(0);

int Redused::get_range_end(int start) {
    int range_id = at(start).range_id;
    for (int i=start+1; i < size; i++) {
        if (at(i).range_id != range_id)
            return i-1;
    }
    return size-1;
}

bool Redused::get_flat_range(int start, int& end) {
    int sum = 0;
    for (end = start; end < size; end++) {
        assert(at(end).state == Pixel::UNKNOWN);
        sum += at(end).real;
        int avrg = sum / (end - start + 1);
        int pv = pix[end].real;
        if (std::abs(pv - avrg) > 2)
            break;
    }
    if (end - start < 3)
        return false;
    int avrg = 0;
    for (int i=start; i < end; i++)
        avrg += pix[i].real;
    avrg /= (end-start);
    range_count += 1;
    for (int i=start; i < end; i++) {
        pix[i].base = avrg;
        pix[i].range_id = range_count;
        pix[i].state = Pixel::FLAT;
    }
    return true;
}
int Redused::maybe_gap_range(int start, int min_gap, int max_gap) {
    int end = start;
    for (; end < size; end++) {
        assert(at(end).state == Pixel::UNKNOWN || at(end).state == Pixel::FLAT);
        if (at(end).state == Pixel::FLAT) {
            int flat_end = get_range_end(end);
            if (flat_end - start > max_gap)
                break;
            end = flat_end;
        }
    }
    if (end == start)
        return end;
    if (end - start >= min_gap && end - start <= max_gap) {
        short base = at(start).real;
        short val_before = at(start-1).real;
        short val_after = at(end).real;
        for (int i = start; i < end; i++)
            base = std::min(base, at(i).real);
        if (val_before >= base+8 && val_after >= base+8) {
            range_count += 1;
            for (int i = start; i < end; i++) {
                pix[i].base = base;
                pix[i].range_id = range_count;
                pix[i].state = Pixel::GAP;
            }
        }
    }
    return end - 1;
}
int Redused::maybe_row_range(int start, int min_row, int max_row) {
    auto& p_bgn = at(start);
    if (p_bgn.state != Pixel::FLAT)
        return start;
    int end = get_range_end(start);
    for (int i=end+1; i < start+min_row; i++) {
        auto& r = at(i);
        if (r.state == Pixel::GAP || r.state == Pixel::ROW)
            return get_range_end(i);
    }
    int row_end = -1;
    for (int i=start+min_row; i < size && i < start+max_row; i++) {
        auto& r = at(i);
        if (r.state == Pixel::GAP || r.state == Pixel::ROW)
            break;
        i = get_range_end(i);
        if (std::abs(r.base - p_bgn.base) > 6)
            continue;
        if (i >= start+max_row)
            break;
        row_end = i;
    }
    if (row_end < 0)
        return end;
    end = row_end;
    range_count += 1;
    short base = p_bgn.base;
    for (int i = start; i < end; i++) {
        if (base > 120)
            base = std::max(base, at(i).base);
        else
            base = std::min(base, at(i).base);
    }
    for (int i = start; i <= end; i++) {
        pix[i].base = base;
        pix[i].range_id = range_count;
        pix[i].state = Pixel::ROW;
    }
    return end;
}
void Redused::detect_flat() {
    for (int i=0; i < size; i++) {
        int end;
        if (get_flat_range(i, end))
            i = end;
    }
}
void Redused::clear_empty() {
    bool have_gaps = false;
    int threshold;
    if (list->header > 0) {
        threshold = 70;
    } else {
        short max_threshold = 100;
        // try to detect threshold from gaps
        for (int i = 0; i < size; i++) {
            if (pix[i].state != Pixel::GAP)
                continue;
            have_gaps = true;
            if (at(i - 1).state == Pixel::ROW)
                max_threshold = std::min(max_threshold, at(i - 1).base);
            i = get_range_end(i);
            if (at(i + 1).state == Pixel::ROW)
                max_threshold = std::min(max_threshold, at(i + 1).base);
        }
        threshold = have_gaps ? max_threshold-2 : 35;
    }
    empty_threshold = threshold;
    int empty_start = 0;
    if (list->row_height <= 0) {
        // if list have no headers - find last gap/row
        for (int i=0; i < size; i++) {
            if (pix[i].state == Pixel::GAP ||pix[i].state == Pixel::ROW) {
                i = get_range_end(i);
                empty_start = i+1;
            }
        }
    }
    //int clear_start = -1;
    for (int i=empty_start; i < size; i++) {
        auto& p = pix[i];
        if (!(p.state == Pixel::FLAT || p.state == Pixel::ROW))
            continue;
        if (p.base > threshold) {
            //clear_start = -1;
            continue;
        }
        //if (clear_start < 0)
        //    clear_start = i;
        short range_id = p.range_id;
        for (; i < size && pix[i].range_id == range_id; i++) {
            pix[i].range_id = 0;
            pix[i].state = Pixel::EMPTY;
        }
        //if (i - clear_start > list->row_height * scale * 1.5) {
        //    // clear the rest
        //    for (i=clear_start; i < size; i++) {
        //        pix[i].range_id = 0;
        //        pix[i].state = Pixel::EMPTY;
        //    }
        //    return;
        //}
        i -= 1;
    }
    return;
}
void Redused::detect_gaps() {
    int gap_min = 1;
    int gap_max = int(std::round(list->row_gap*scale))+3;
    for (int i=0; i < size; i++) {
        if (pix.at(i).state != Pixel::UNKNOWN)
            continue;
        i = maybe_gap_range(i, gap_min, gap_max);
    }
}
bool Redused::insert_gaps() {
    int gap_max = std::max(2, int(std::round(list->row_gap*scale))+2);
    bool added = false;
    for (int i=0; i < size; i++) {
        if (pix.at(i).state != Pixel::ROW)
            continue;
        // ensure gap before this row
        if (i > 0 && pix.at(i-1).state != Pixel::GAP) {
            int g = i-1;
            for (int j=1; j <= gap_max; j++) {
                if (i-j < 0)
                    break;
                if (pix.at(i-j).state != Pixel::UNKNOWN)
                    break;
                g = i - j;
            }
            added = true;
            short base = at(i-1).real;
            for (int j=g; j < i; j++)
                base = std::min(base, at(j).real);
            range_count += 1;
            for (int j=g; j < i; j++) {
                pix[j].base = base;
                pix[j].range_id = range_count;
                pix[j].state = Pixel::GAP;
                pix[j].artificial = true;
            }
        }
        // ensure gap after this row
        i = get_range_end(i);
        if (i < size-1 && pix.at(i+1).state != Pixel::GAP) {
            int g = i+1;
            for (int j=1; j <= gap_max; j++) {
                if (i+j >= size)
                    break;
                if (pix.at(i+j).state != Pixel::UNKNOWN)
                    break;
                g = i + j;
            }
            added = true;
            short base = at(i+1).real;
            for (int j=i+1; j <= g; j++)
                base = std::min(base, at(j).real);
            range_count += 1;
            for (int j=i+1; j <= g; j++) {
                pix[j].base = base;
                pix[j].range_id = range_count;
                pix[j].state = Pixel::GAP;
                pix[j].artificial = true;
            }
        }
    }
    return added;
}
void Redused::detect_rows() {
    int height_min = int(std::round((list->row_height-list->row_gap)*scale))-2;
    int height_max = int(std::round((list->row_height+list->row_gap)*scale))+2;
    for (int i=0; i < size; i++) {
        if (pix.at(i).state != Pixel::FLAT)
            continue;
        i = maybe_row_range(i, height_min, height_max);
    }
}
bool Redused::insert_rows() {
    int height_min = int(std::round(list->row_height*scale))-3;
    int height_max = int(std::round(list->row_height*scale))+3;
    bool added = false;
    for (int i=0; i < size-2; i++) {
        if (at(i).state != Pixel::GAP)
            continue;
        if (at(i-1).state == Pixel::UNKNOWN || at(i-1).state == Pixel::FLAT) {
            // ensure row before this gap
            int r = i - 1;
            for (int j = i - 1; j >= 0 && i - j <= height_max; j--) {
                if (!(pix.at(j).state == Pixel::UNKNOWN || pix.at(j).state == Pixel::FLAT))
                    break;
                r = j;
            }
            if (r < 0)
                r = 0;
            if (i - r - 1 >= height_min && i - r - 1 <= height_max) {
                added = true;
                short base = at(r).base;
                for (int j = r; j < i; j++)
                    base = std::min(base, at(r).base);
                range_count += 1;
                for (int j = r; j < i; j++) {
                    pix[j].base = base;
                    pix[j].range_id = range_count;
                    pix[j].state = Pixel::ROW;
                    pix[j].artificial = true;
                }
            }
        }
        // skip to the end of this gap
        i = get_range_end(i);
        if (at(i+1).state == Pixel::EMPTY)
            continue;
        if (at(i+1).state == Pixel::ROW || at(i+1).state == Pixel::GAP) {
            // already have row after this gap
            i = get_range_end(i+1);
            continue;
        }
        // ensure row after this gap
        int r = i + 1;
        for (int j = i + 1; j < size && j - i <= height_max; j++) {
            if (!(pix.at(j).state == Pixel::UNKNOWN || pix.at(j).state == Pixel::FLAT))
                break;
            r = j;
        }
        if (r >= size)
            r = size - 1;
        if (r - i - 1 >= height_min && r - i - 1 <= height_max) {
            added = true;
            short base = at(r).base;
            for (int j = i + 1; j <= r; j++)
                base = std::min(base, at(r).base);
            range_count += 1;
            for (int j = i + 1; j <= r; j++) {
                pix[j].base = base;
                pix[j].range_id = range_count;
                pix[j].state = Pixel::ROW;
                pix[j].artificial = true;
            }
        }
        i = r;
    }
    return added;
}

bool Redused::normalize_rows() {
    bool changed = false;
    int row_norm = int(std::round(list->row_height*scale));
    for (int i=0; i < size; i++) {
        if (pix[i].state != Pixel::ROW)
            continue;
        int min_val = pix[i].base - 10;
        int row_bgn = i;
        int row_end = get_range_end(i);
        int row_size = row_end - row_bgn + 1;
        if (row_size < row_norm+1) {
            auto& prv = at(row_bgn-1);
            auto& nxt = at(row_end+1);
            bool can_ext_before = (prv.real >= min_val && prv.state == Pixel::GAP && at(row_bgn-2).state != Pixel::ROW);
            bool can_ext_after  = (nxt.real >= min_val && nxt.state == Pixel::GAP && at(row_end+2).state != Pixel::ROW);
            if (can_ext_before && can_ext_after)
                can_ext_before = (prv.real < nxt.real);
            if (can_ext_after) {
                changed = true;
                pix[row_end+1].state = Pixel::ROW;
                pix[row_end+1].base = pix[row_end].base;
                pix[row_end+1].range_id = pix[row_end].range_id;
            }
            if (can_ext_before) {
                changed = true;
                pix[row_bgn-1].state = Pixel::ROW;
                pix[row_bgn-1].base = pix[row_bgn].base;
                pix[row_bgn-1].range_id = pix[row_bgn].range_id;
            }
        }
        else if (row_size > row_norm+1) {
            auto& fst = at(row_bgn);
            auto& lst = at(row_end);
        }
        i = get_range_end(row_end);
    }
    return changed;
}

//#define DEBUG_LIST_DETECTOR 1

bool List::detect(DetectParams& params) {
    ClassifyEnv& env = params.env;
    cv::Rect listReferenceRect = env.calcReferenceRect(this->rect);
    cv::Rect listCapturedRect =  env.cvtReferenceToCaptured(listReferenceRect);
    env.cropToCapture(listCapturedRect);
    if (listCapturedRect.empty())
        return false;

    env.classified.emplace_back(ClsDetType::Widget, env.isWarpMode(), this->name, listReferenceRect);
    ClassifiedRect& clsListRect = env.classified.back();
    clsListRect.u.widg.referenceRect = listReferenceRect;
    clsListRect.u.widg.ws = WState::Unknown;
    clsListRect.u.widg.widget = this;

    if (row_height <= 0)
        return true;

    XMat reducedImage;
    cv::reduce(env.getGrayImage()(listCapturedRect), reducedImage, 1, cv::REDUCE_AVG, CV_8UC1);
    cv::Mat reducedMat = toMat(reducedImage);
    Redused reduced(this, reducedMat.data, listCapturedRect.height, env.getScale());
    reduced.detect_flat();
    reduced.detect_gaps();
    reduced.detect_rows();
    reduced.clear_empty();
    for (bool added = true; added;) {
        added = reduced.insert_gaps();
        added |= reduced.insert_rows();
    }
    while (reduced.normalize_rows())
        ;
#ifdef  DEBUG_LIST_DETECTOR
    cv::Mat histImage(256, reduced.size, CV_8UC1, cv::Scalar(0));
    cv::line(histImage,
             cv::Point(0, reduced.empty_threshold),
             cv::Point(reduced.size, reduced.empty_threshold),
             cv::Scalar(90));
    for (int i=0; i < reduced.size; i++) {
        auto& p = reduced.at(i);
        if (p.state == Pixel::FLAT)
            histImage.at<uchar>(0, i) = 180;
        if (p.state == Pixel::GAP)
            histImage.at<uchar>(2, i) = p.artificial ? 120 : 180;
        else if (p.state == Pixel::ROW)
            histImage.at<uchar>(4, i) = p.artificial ? 120 : 255;
        histImage.at<uchar>(p.base, i) = 128;
        histImage.at<uchar>(p.real, i) = 255;
    }
#endif

    bool has_rows = false;
    for (int i=0; i < reduced.size; i++) {
        if (reduced.pix[i].state != Pixel::ROW)
            continue;
        int row_bgn = i;
        int row_end = reduced.get_range_end(i);
        int row_val = reduced.pix[i].base;
        i = row_end;
        has_rows = true;

        cv::Rect rowCapturedRect {0, row_bgn, listCapturedRect.width, row_end-row_bgn+1};
        rowCapturedRect += listCapturedRect.tl();
        cv::Rect rowReferenceRect = env.cvtCapturedToReference(rowCapturedRect);

        WState ws = WState::Unknown;
        if (row_val > 120) // bright = focused
            ws = WState::Focused;
        else
            ws = WState::Normal;

#ifdef  DEBUG_LIST_DETECTOR
        cv::Mat rowImage = toMat(env.getGrayImage()(rowCapturedRect));
#endif
        env.classified.emplace_back(ClsDetType::ListRow, env.isWarpMode(), "", rowReferenceRect);
        ClassifiedRect& clsRowRect = env.classified.back();
        clsRowRect.u.lrow.capturedRect = rowCapturedRect;
        clsRowRect.u.lrow.list = this;
        clsRowRect.u.lrow.ws = ws;
        clsRowRect.u.lrow.text_confidence = -1;
        if (ws == WState::Focused) {
            clsListRect.u.widg.ws = WState::Focused;
            clsRowRect.u.lrow.capturedRect.y += 1;
            clsRowRect.u.lrow.capturedRect.height -= 1;
            if (!params.uiState.focused)
                params.uiState.focused = this;
        }
    }
    return has_rows;
}


bool Screen::checkStatus() const {
    if (!status.is_object())
        return false;
    for (auto& kv : status.as_object()) {
        auto& key = kv.first;
        auto& val = kv.second;
        if (key == "gui" || key == "focus") {
            auto gf = enum_cast<GuiFocus>(val.as_string());
            LOG_IF(!gf.has_value(),ERROR) << "Bad gui focus name: " << val;
            if (gf.value() != st::guiFocus)
                return false;
            continue;
        }
        if (key == "ship") {
            std::string ship = toLower(st::shipInfo.shipType);
            bool ok = false;
            if (val.is_string()) {
                ok = (val.as_string() == ship);
            }
            else if (val.is_array()) {
                for (auto& s : val.as_array()) {
                    if (s.as_string() == ship) {
                        ok = true;
                        break;
                    }
                }
            }
            if (!ok)
                return false;
            continue;
        }
        if (key == "docked") {
            if (val.as_boolean() != st::ship.flags.docked)
                return false;
            continue;
        }
        LOG(ERROR) << "Unknown or unimplemented status key: " << key;
        return false;
    }
    return true;
}

class ExprPoint : public EvalPoint {
public:
    ExprPoint(const json5pp::value& source);
    cv::Point calcReferencePoint(const ResolvedEnv& env) const override;

private:
    int eval(const spAst& ast, const ResolvedEnv& env) const;

    static peg::parser& initParser();

    const json5pp::value source;
    std::array<std::variant<int,spAst>,2> astPoint;
};

class ExprRect : public EvalRect {
public:
    ExprRect(const json5pp::value& source);
    cv::Rect calcReferenceRect(const ResolvedEnv& env) const override;

private:
    int eval(const spAst& ast, const ResolvedEnv& env) const;

    static peg::parser& initParser();

    const json5pp::value source;
    std::array<std::variant<int,spAst>,4> astRect;
};

class ExprLine : public EvalLine {
public:
    ExprLine(const json5pp::value& source);
    cv::Line calcReferenceLine(const ResolvedEnv& env) const override;

private:
    int eval(const spAst& ast, const ResolvedEnv& env) const;

    static peg::parser& initParser();

    const json5pp::value source;
    std::array<std::variant<int,spAst>,4> astLine;
};


ExprPoint::ExprPoint(const json5pp::value& src)
        : source(src)
{
    if (!src.is_array() || src.as_array().size() != 4) {
        LOG(ERROR) << "Bad point: " << src;
        return;
    }
    peg::parser& parser = initParser();
    if (!parser)
        return;

    for (int i=0; i < 4; i++) {
        auto& v = source.at(i);
        if (v.is_integer()) {
            astPoint[i] = v.as_integer();
            continue;
        }
        else if (v.is_string()) {
            spAst ast;
            bool ok = parser.parse(v.as_string(), ast);
            if (ok) {
                astPoint[i] = parser.optimize_ast(ast);
                continue;
            }
        }
        LOG(ERROR) << "Bad value: " << v << " in point " << source;
    }
}

ExprRect::ExprRect(const json5pp::value& src)
    : source(src)
{
    if (!src.is_array() || src.as_array().size() != 4) {
        LOG(ERROR) << "Bad rect: " << src;
        return;
    }
    peg::parser& parser = initParser();
    if (!parser)
        return;

    for (int i=0; i < 4; i++) {
        auto& v = source.at(i);
        if (v.is_integer()) {
            astRect[i] = v.as_integer();
            continue;
        }
        else if (v.is_string()) {
            spAst ast;
            bool ok = parser.parse(v.as_string(), ast);
            if (ok) {
                astRect[i] = parser.optimize_ast(ast);
                continue;
            }
        }
        LOG(ERROR) << "Bad value: " << v << " in rect " << source;
    }
}

ExprLine::ExprLine(const json5pp::value& src)
        : source(src)
{
    if (!src.is_array() || src.as_array().size() != 4) {
        LOG(ERROR) << "Bad line: " << src;
        return;
    }
    peg::parser& parser = initParser();
    if (!parser)
        return;

    for (int i=0; i < 4; i++) {
        auto& v = source.at(i);
        if (v.is_integer()) {
            astLine[i] = v.as_integer();
            continue;
        }
        else if (v.is_string()) {
            spAst ast;
            bool ok = parser.parse(v.as_string(), ast);
            if (ok) {
                astLine[i] = parser.optimize_ast(ast);
                continue;
            }
        }
        LOG(ERROR) << "Bad value: " << v << " in rect " << source;
    }
}

cv::Point ExprPoint::calcReferencePoint(const ResolvedEnv& env) const {
    cv::Point point;
    for (int i=0; i < 2; i++) {
        int* ptr = &point.x;
        if (holds_alternative<int>(astPoint[i]))
            ptr[i] = std::get<int>(astPoint[i]);
        else
            ptr[i] = eval(std::get<spAst>(astPoint[i]), env);
    }
    return point;
}

cv::Rect ExprRect::calcReferenceRect(const ResolvedEnv& env) const {
    cv::Rect rect;
    for (int i=0; i < 4; i++) {
        int* ptr = &rect.x;
        if (holds_alternative<int>(astRect[i]))
            ptr[i] = std::get<int>(astRect[i]);
        else
            ptr[i] = eval(std::get<spAst>(astRect[i]), env);
    }
    return rect;
}

cv::Line ExprLine::calcReferenceLine(const ResolvedEnv& env) const {
    cv::Line line;
    for (int i=0; i < 4; i++) {
        int* ptr = &line.x0;
        if (holds_alternative<int>(astLine[i]))
            ptr[i] = std::get<int>(astLine[i]);
        else
            ptr[i] = eval(std::get<spAst>(astLine[i]), env);
    }
    return line;
}

static peg::parser& init_parser() {
    static peg::parser parser;
    if (!parser) {
        parser.load_grammar(R"(
        Expr        <-  Term (TermOp Term)*
        Term        <-  Factor (FactorOp Factor)*
        Factor      <-  Num / Ident / '(' Expr ')'

        TermOp      <-  < [-+] >
        FactorOp    <-  < [/*] >

        Num         <- < '-'? [0-9]+ >
        Ident       <- < [a-zA-Z] [a-zA-Z0-9-_$.]* >
        %whitespace <- [ \t\r\n]*
        )");
        if (parser)
            parser.enable_ast();
        else
            LOG(ERROR) << "Expression parser initialization error";
    }
    return parser;
}

peg::parser& ExprPoint::initParser() {
    return init_parser();
}

peg::parser& ExprRect::initParser() {
    return init_parser();
}

peg::parser& ExprLine::initParser() {
    return init_parser();
}

static int getIntValue(const std::string_view& view, const ResolvedEnv& env) {
    size_t dot = view.find('.');
    if (dot == std::string_view::npos) {
        if (equalsIgnoreCase(view, "ScreenWidth"))
            return env.ReferenceScreenSize.width;
        if (equalsIgnoreCase(view, "ScreenHeight"))
            return env.ReferenceScreenSize.height;
        LOG(ERROR) << "Unknown identifier in expression: " << view;
        return 0;
    }
    const ClassifiedRect* cr = nullptr;
    const std::string_view& name = view.substr(0,dot);
    cv::Point offset;
    for (auto& it : env.classified) {
        if (it.cdt == ClsDetType::Detected && name == it.text) {
            cr = &it;
            offset = cr->detectedRect.tl() - cr->u.tdet.referenceRect.tl();
            break;
        }
        if (it.cdt == ClsDetType::LineDetected && it.text.starts_with(name) && it.text[name.size()] == ':') {
            cr = &it;
            offset = cr->u.ldet.offset;
            break;
        }
        if (it.cdt == ClsDetType::Widget && name == it.text) {
            cr = &it;
            offset = cr->detectedRect.tl() - cr->u.widg.referenceRect.tl();
            break;
        }
    }
    if (!cr) {
        LOG(ERROR) << "Identifier for detector '" << name << "' not found in classified rects";
        return 0;
    }
    const std::string_view& field = view.substr(dot+1);
    if (field == "x" || field == "l" || field == "left")
        return cr->detectedRect.x;
    if (field == "y" || field == "t" || field == "top")
        return cr->detectedRect.y;
    if (field == "w" || field == "width")
        return cr->detectedRect.width;
    if (field == "h" || field == "height")
        return cr->detectedRect.height;
    if (field == "r" || field == "right")
        return cr->detectedRect.br().x;
    if (field == "b" || field == "bottom")
        return cr->detectedRect.br().y;
    if (field == "cx" || field == "center_x")
        return cr->detectedRect.x + cr->detectedRect.width/2;
    if (field == "cy" || field == "center_y")
        return cr->detectedRect.y + cr->detectedRect.height/2;
    if (field == "ox" || field == "offset_x")
        return offset.x;
    if (field == "oy" || field == "offset_y")
        return offset.y;
    LOG(ERROR) << "Field " << field << " not known, use x,y,w,h,l,t,r,b and cx,cy, ox, oy";
    return 0;
}

static int eval_ast(const spAst& ast, const ResolvedEnv& env) {
    if (ast->name == "Num") {
        return ast->token_to_number<int>();
    }
    else if (ast->name == "Ident") {
        return getIntValue(ast->token, env);
    }
    else {
        const auto &nodes = ast->nodes;
        auto result = eval_ast(nodes[0], env);
        for (auto i = 1u; i < nodes.size(); i += 2) {
            auto num = eval_ast(nodes[i + 1], env);
            auto ope = nodes[i]->token[0];
            switch (ope) {
            case '+': result += num; break;
            case '-': result -= num; break;
            case '*': result *= num; break;
            case '/': result /= num; break;
            default:
                LOG(ERROR) << "Bad operator '" << ope << "'";
            }
        }
        return result;
    }
}

int ExprPoint::eval(const spAst& ast, const ResolvedEnv& env) const {
    return eval_ast(ast, env);
}

int ExprRect::eval(const spAst& ast, const ResolvedEnv& env) const {
    return eval_ast(ast, env);
}

int ExprLine::eval(const spAst& ast, const ResolvedEnv& env) const {
    return eval_ast(ast, env);
}

}

spEvalPoint makeEvalPoint(const widget::Widget& widget, const char* name, const json5pp::value& jv) {
    if (jv.is_string()) {
        std::vector<std::string> scope_name = split(jv.as_string(), ':');
        if (scope_name.size() != 2) {
            LOG(ERROR) << "Reference must be at form 'scope:name', but is " << jv;
            return {};
        }
        const std::string& scope = scope_name[0];
        for (const widget::Widget* p=&widget; p; p = p->parent) {
            if (p->tp == widget::WidgetType::Screen || p->tp == widget::WidgetType::Mode || p->tp == widget::WidgetType::Dialog) {
                auto dlg = (const widget::BaseDialog*) p;
                if (dlg->varSetMap.contains(scope)) {
                    return std::make_shared<RefPoint>(*dlg, scope_name[1], scope);
                }
            }
        }
        LOG(ERROR) << "Cannot resolve var scope " << scope << " in parents of widget " << widget.path;
        return {};
    }
    if (!jv.is_array() || jv.as_array().size() != 2) {
        LOG(ERROR) << "For point '" << name << "' expecting array of 2 ints, but got: " << jv;
        return {};
    }
    auto& j_arr = jv.as_array();
    bool simple = true;
    for (int i=0; i < 4; i++) {
        if (!j_arr[i].is_integer()) {
            simple = false;
            if (!j_arr[i].is_string())
                return {};
        }
    }
    if (simple) {
        cv::Point p;
        p.x = j_arr[0].as_integer();
        p.y = j_arr[1].as_integer();
        return std::make_shared<ConstPoint>(p);
    }
    return std::make_shared<widget::ExprPoint>(jv);
}

spEvalRect makeEvalRect(const widget::Widget& widget, const char* name, const json5pp::value& jv) {
    if (jv.is_string()) {
        std::vector<std::string> scope_name = split(jv.as_string(), ':');
        if (scope_name.size() != 2) {
            LOG(ERROR) << "Reference must be at form 'scope:name', but is " << jv;
            return {};
        }
        const std::string& scope = scope_name[0];
        for (const widget::Widget* p=&widget; p; p = p->parent) {
            if (p->tp == widget::WidgetType::Screen || p->tp == widget::WidgetType::Mode || p->tp == widget::WidgetType::Dialog) {
                auto dlg = (const widget::BaseDialog*) p;
                if (dlg->varSetMap.contains(scope)) {
                    return std::make_shared<RefRect>(*dlg, scope_name[1], scope);
                }
            }
        }
        LOG(ERROR) << "Cannot resolve var scope " << scope << " in parents of widget " << widget.path;
        return {};
    }
    if (!jv.is_array() || jv.as_array().size() != 4) {
        LOG(ERROR) << "For rect '" << name << "' expecting array of 4 ints, but got: " << jv;
        return {};
    }
    auto& j_arr = jv.as_array();
    bool simple = true;
    for (int i=0; i < 4; i++) {
        if (!j_arr[i].is_integer()) {
            simple = false;
            if (!j_arr[i].is_string())
                return {};
        }
    }
    if (simple) {
        cv::Rect r;
        r.x = j_arr[0].as_integer();
        r.y = j_arr[1].as_integer();
        r.width = j_arr[2].as_integer();
        r.height = j_arr[3].as_integer();
        return std::make_shared<ConstRect>(r);
    }
    return std::make_shared<widget::ExprRect>(jv);
}

spEvalLine makeEvalLine(const widget::Widget& widget, const char* name, const json5pp::value& jv) {
    if (jv.is_string()) {
        std::vector<std::string> scope_name = split(jv.as_string(), ':');
        if (scope_name.size() != 2) {
            LOG(ERROR) << "Reference must be at form 'scope:name', but is " << jv;
            return {};
        }
        const std::string& scope = scope_name[0];
        for (const widget::Widget* p=&widget; p; p = p->parent) {
            if (p->tp == widget::WidgetType::Screen || p->tp == widget::WidgetType::Mode || p->tp == widget::WidgetType::Dialog) {
                auto dlg = (const widget::BaseDialog*) p;
                if (dlg->varSetMap.contains(scope)) {
                    return std::make_shared<RefLine>(*dlg, scope_name[1], scope);
                }
            }
        }
        LOG(ERROR) << "Cannot resolve var scope " << scope << " in parents of widget " << widget.path;
        return {};
    }
    if (!jv.is_array() || jv.as_array().size() != 4) {
        LOG(ERROR) << "For rect '" << name << "' expecting array of 4 ints, but got: " << jv;
        return {};
    }
    auto& j_arr = jv.as_array();
    bool simple = true;
    for (int i=0; i < 4; i++) {
        if (!j_arr[i].is_integer()) {
            simple = false;
            if (!j_arr[i].is_string())
                return {};
        }
    }
    if (simple) {
        cv::Line ln;
        ln.x0 = j_arr[0].as_integer();
        ln.y0 = j_arr[1].as_integer();
        ln.x1 = j_arr[2].as_integer();
        ln.y1 = j_arr[3].as_integer();
        return std::make_shared<ConstLine>(ln);
    }
    return std::make_shared<widget::ExprLine>(jv);
}

cv::Point RefPoint::calcReferencePoint(const ResolvedEnv& detectorState) const {
    const std::string& ship = st::shipInfo.shipType;
    auto& varSet = mDlg.varSetMap.at(mScope);
    for (auto& vars : varSet) {
        if (vars.keys.empty() || std::count(vars.keys.begin(),vars.keys.end(), ship)) {
            auto& vals = vars.values.at(mName);
            cv::Point point {(int)vals[0],(int)vals[1]};
            return point;
        }
    }
    return {};
}

cv::Rect RefRect::calcReferenceRect(const ResolvedEnv& detectorState) const {
    const std::string& ship = st::shipInfo.shipType;
    auto& varSet = mDlg.varSetMap.at(mScope);
    for (auto& vars : varSet) {
        if (vars.keys.empty() || std::count(vars.keys.begin(),vars.keys.end(), ship)) {
            auto& vals = vars.values.at(mName);
            cv::Rect rect {(int)vals[0],(int)vals[1],(int)vals[2],(int)vals[3]};
            return rect;
        }
    }
    return {};
}

cv::Line RefLine::calcReferenceLine(const ResolvedEnv& detectorState) const {
    const std::string& ship = st::shipInfo.shipType;
    auto& varSet = mDlg.varSetMap.at(mScope);
    for (auto& vars : varSet) {
        if (vars.keys.empty() || std::count(vars.keys.begin(),vars.keys.end(), ship)) {
            auto& vals = vars.values.at(mName);
            cv::Line line {(int)vals[0],(int)vals[1],(int)vals[2],(int)vals[3]};
            return line;
        }
    }
    return {};
}
