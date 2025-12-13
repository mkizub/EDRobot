//
// Created by mkizub on 31.05.2025.
//

#include "pch.h"

#include <peglib/peglib.h>

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
        double scale = double(ocr_bot - ocr_top) / double(ocr::ASCENT+ocr::DESCENT);
        int reference_line_height = (int) std::round(ocr::LINE_HEIGHT * scale);
        int lines = (int) std::round(double(r.height) / double(reference_line_height));
        //r.height = lines * reference_line_height;
        //if (lines > 1) {
        //    r.x -= (int) std::round(scale*ocr::LEADING/2);
        //    //r.height += (int) std::round(scale*ocr::LEADING);
        //}
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
    cv::Rect captureR = env.cvtReferenceToCaptured(expectedR);

    if (!icon.empty()) {
        if (!detector) {
            detector = std::make_unique<detect::ImageTemplate>(icon, nullptr);
            detector->extendLT = extendLT;
            detector->extendRB = extendRB;
        }
        // assume icon is at the center of this bgutton
        auto& orig = detector->refOrig;
        auto& size = detector->refSize;
        orig.x = expectedR.x + expectedR.width/2 - size.width/2;
        orig.y = expectedR.y + expectedR.height/2 - size.height/2;
        if (detector->match(params.env) < 0.5)
            return false;
        int cx = detector->captureRect.x + detector->captureRect.width/2;
        int cy = detector->captureRect.y + detector->captureRect.height/2;
        captureR.x = cx - captureR.width/2;
        captureR.y = cy - captureR.height/2;
        detectedR = env.cvtCapturedToReference(captureR);
    }
    else if (extendLT != cv::Point() || extendRB != cv::Point()) {
        cv::Rect extendR = {expectedR.tl() - extendLT, expectedR.br() + extendRB};
        cv::Rect matchR = env.cvtReferenceToCaptured(extendR);

        //cv::Vec3b hsvColorMin {5, 127, 30};
        //cv::Vec3b hsvColorMax {30, 255, 255};
        //XMat hsvImage;
        //cv::cvtColor(env.getColorImage()(matchR), hsvImage, cv::COLOR_BGR2HSV);
        //XMat thrImage;
        //cv::inRange(hsvImage, hsvColorMin, hsvColorMax, thrImage);
        XMat grayImage = env.getGrayImage()(matchR);
        detect::LaplacianFilter laplFilter(5);
        XMat laplImage = laplFilter.apply(grayImage, {});
        detect::ThresholdFilter thrFilter;
        XMat thrImage = thrFilter.apply(laplImage, {});

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

struct Redused {
    enum State { UNKNOWN, FLAT, GAP, ROW, HDR, EMPTY };
    struct Range {
        State state;
        uint8_t val;
        uint8_t min;
        uint8_t max;
        uint8_t avr;
        short bgn;
        short end;
        bool artificial;

        short len() { return end-bgn+1; }
    };

    static Range emptyRange;
    const List* list;
    const int size;
    const double scale;
    const uchar* pix;
    std::vector<Range> rng;
    int range_count;
    int empty_threshold;
    Redused(List* list, int len, uchar* pixels, double scale)
        : list(list)
        , size(len)
        , pix(pixels)
        , scale(scale)
        , range_count(0)
        , empty_threshold(100)
    {
        rng.reserve(len);
        for (int i=0; i < len; i++)
            rng.emplace_back(UNKNOWN, pix[i], pix[i], pix[i], pix[i], i, i, false);
        if (list->header > 0)
            empty_threshold = 70;
    }
    const Range& at(int i) {
        if (i < 0 || i >= size)
            return emptyRange;
        return rng[i];
    }
    void make_range(State state, int bgn, int end, bool artificial=false);
    bool maybe_flat_range(int bgn, int& end, int& rate);
    void detect_flat();
    bool maybe_gap_range(int bgn, int max_gap);
    void detect_gaps();
    bool maybe_row_range(int bgn, int min_row, int max_row);
    void detect_rows();
    void calc_threshold();
    void clear_empty();
    bool insert_gaps();
    bool insert_rows();
    bool normalize_rows();
};

Redused::Range Redused::emptyRange {EMPTY, 0, 0, 0, 0, 0, 0, false};

void Redused::make_range(State state, int bgn, int end, bool artificial) {
    assert (end >= bgn);
    int sum = pix[bgn];
    uint8_t min = pix[bgn];
    uint8_t max = pix[bgn];
    for (int i=bgn+1; i <= end; i++) {
        auto pv = pix[i];
        sum += pv;
        min = std::min(min, pv);
        max = std::max(max, pv);
    }
    uint8_t avr = sum / (end - bgn + 1);
    uint8_t val = min;
    if (state == ROW) {
        if (avr > min+4)
            val = avr - 4;
    }
    Range r {state, val, min, max, avr, short(bgn), short(end), artificial};
    for (int i=bgn; i <= end; i++) {
        rng[i] = r;
    }
}

bool Redused::maybe_flat_range(int bgn, int& end, int& rate) {
    rate = 0;
    uint8_t min = rng[bgn].val;
    uint8_t max = min;
    int sum = 0;
    for (end = bgn; end < size; end++) {
        auto& r = rng[end];
        assert(r.state == UNKNOWN);
        auto rv = rng[end].val;
        min = std::min(min, rv);
        max = std::max(max, rv);
        if (max - min > 3)
            break;
        sum += rv;
        int avr = sum / (end - bgn + 1);
        if (std::abs(rv - avr) > 2)
            break;
    }
    if (end - bgn < 3)
        return false;
    for (int i=bgn; i < end; i++) {
        int rv = rng[i].val;
        sum += rv;
        if (i == bgn)
            rate = 1;
        else if (rv == rng[i-1].val)
            rate += 2;
        else if (std::abs(rv - rng[i-1].val) < 2)
            rate += 1;
    }
    end -= 1;
    return true;
}
void Redused::detect_flat() {
    for (int i=0; i < size; i++) {
        int i_end;
        int i_rate;
        if (maybe_flat_range(i, i_end, i_rate)) {
            int best_rate = i_rate;
            int b_bgn = i;
            int b_end = i_end;
            for (int j=i+1; j <= i_end; j++) {
                int j_end;
                int j_rate;
                if (maybe_flat_range(j, j_end, j_rate)) {
                    if (j_rate > best_rate) {
                        best_rate = j_rate;
                        b_bgn = j;
                        b_end = j_end;
                    }
                }
            }
            if (b_bgn == i) {
                make_range(FLAT, i, i_end);
                i = rng[i].end;
            } else if (b_bgn - i > 3) {
                make_range(FLAT, i, b_bgn-1);
                make_range(FLAT, b_bgn, b_end);
                i = b_end;
            } else {
                make_range(FLAT, i, b_end);
                i = b_end;
            }
        }
    }
}

bool Redused::maybe_gap_range(int bgn, int max_gap) {
    if (rng[bgn].len() > max_gap)
        return false;
    const int DELTA = 8;
    int end = -1;
    uint8_t min = 255;
    uint8_t rise = 0;
    for (int i=bgn; i < size; i++) {
        auto& r = rng[i];
        assert(r.state == UNKNOWN || r.state == FLAT);
        if (r.end - bgn > max_gap)
            break;
        if (rise > 0 && r.min < rise)
            break;
        if (r.max > min)
            rise = std::max(rise, r.max);
        end = r.end;
        min = std::min(min, r.min);
        i = r.end;
    }
    if (end < bgn)
        return false;
    assert (end - bgn <= max_gap);
    auto& r_prev = at(bgn-1);
    auto& r_next = at(end+1);
    bool is_gap = r_prev.max > min+DELTA && r_next.max > min+DELTA;
    is_gap |= r_prev.min < min+4 && r_next.max > r_prev.min+2*DELTA;
    is_gap |= r_prev.max > min+2*DELTA && r_next.min < min+4;
    if (!is_gap)
        return false;
    make_range(GAP, bgn, end);
    return true;
}
void Redused::detect_gaps() {
    int height_min = int(std::round((list->row_height-list->row_gap)*scale))-3;
    int gap_max = int(std::round(list->row_gap*scale))+2;
    for (int i=0; i < size; i++) {
        auto& r = rng[i];
        assert (r.state == UNKNOWN || r.state == FLAT);
        if (maybe_gap_range(i, gap_max))
            i = rng[i].end + height_min;
        else
            i = rng[i].end;
    }
}

bool Redused::maybe_row_range(int bgn, int min_row, int max_row) {
    auto& r_bgn = rng[bgn];
    if (r_bgn.state != FLAT || r_bgn.bgn+min_row >= size)
        return false;
    int limit = std::min(size, bgn+min_row);
    int i = bgn;
    for (; i < limit; i++) {
        auto& r = rng[i];
        assert (r.state == UNKNOWN || r.state == FLAT || r.state == GAP);
        if (r.state == GAP)
            return false;
        i = r.end;
    }
    limit = std::min(size, bgn+max_row);
    if (i > limit)
        return false;
    int row_val = r_bgn.val;
    int row_end = i-1;
    for (; i < limit; i++) {
        auto& r = rng[i];
        assert (r.state == UNKNOWN || r.state == FLAT || r.state == GAP);
        if (r.state == GAP)
            break;
        if (r.end >= limit)
            break;
        row_end = r.end;
        i = r.end;
    }
    int end = row_end;
    make_range(ROW, bgn, row_end);
    if (r_bgn.min > 120) {
        for (i=r_bgn.bgn; i <= r_bgn.end; i++)
            rng[i].val = rng[i].max;
    }
    return end;
}
void Redused::detect_rows() {
    int height_min = int(std::round((list->row_height-list->row_gap)*scale))-3;
    int height_max = int(std::round((list->row_height+list->row_gap)*scale))+3;
    for (int i=0; i < size; i++) {
        if (rng[i].state != FLAT)
            continue;
        maybe_row_range(i, height_min, height_max);
        i = rng[i].end;
    }
}

void Redused::calc_threshold() {
    int threshold;
    if (list->header > 0) {
        threshold = 60;
    } else {
        bool have_gaps = false;
        uint8_t max_threshold = 100;
        // try to detect threshold from gaps
        for (int i = 0; i < size; i++) {
            if (rng[i].state != GAP) {
                i = rng[i].end;
                continue;
            }
            auto& prv = at(i-1);
            if (prv.state == ROW && prv.min > rng[i].val+4) {
                max_threshold = std::min(max_threshold, prv.val);
                have_gaps = true;
            }
            i = at(i).end;
            auto& nxt = at(i+1);
            if (nxt.state == ROW && nxt.min > rng[i].val+4) {
                max_threshold = std::min(max_threshold, nxt.val);
                have_gaps = true;
            }
        }
        threshold = have_gaps ? max_threshold-2 : 35;
    }
    empty_threshold = threshold;
}
void Redused::clear_empty() {
    int empty_start = 0;
    if (list->header <= 0) {
        // no headers, ensure no empty space between rows
        for (int i=0; i < size; i++) {
            auto& r = rng[i];
            if (r.state == ROW && r.val > empty_threshold)
                empty_start = r.end + 1;
            i = r.end;
        }
    }
    for (int i=empty_start; i < size; i++) {
        auto& r = rng[i];
        if (r.state != GAP) {
            if (r.val <= empty_threshold) {
                for (int j=r.bgn; j <= r.end; j++) {
                    rng[j].state = EMPTY;
                }
            }
        }
        i = r.end;
    }
}
bool Redused::insert_gaps() {
    int gap_max = std::max(2, int(std::round(list->row_gap*scale))+2);
    bool added = false;
    for (int i=0; i < size; i++) {
        if (rng[i].state != ROW)
            continue;
        // ensure gap before this row
        if (i > 0 && rng[i-1].state == UNKNOWN) {
            int g = i-1;
            for (int j=1; j <= gap_max; j++) {
                if (i-j < 0)
                    break;
                if (at(i-j).state != UNKNOWN)
                    break;
                g = i - j;
            }
            added = true;
            make_range(GAP, g, i-1, true);
        }
        // ensure gap after this row
        i = rng[i].end;
        if (i < size-1 && rng[i+1].state == UNKNOWN) {
            int g = i+1;
            for (int j=1; j <= gap_max; j++) {
                if (i+j >= size)
                    break;
                if (rng[i+j].state != UNKNOWN)
                    break;
                g = i + j;
            }
            added = true;
            make_range(GAP, i+1, g, true);
        }
    }
    return added;
}
bool Redused::insert_rows() {
    int height_min = int(std::round(list->row_height*scale))-3;
    int height_max = int(std::round(list->row_height*scale))+3;
    bool added = false;
    int r;
    for (int i=0; i < size-2; i++) {
        if (rng[i].state != GAP)
            continue;
        if (at(i-1).state == UNKNOWN || at(i-1).state == FLAT) {
            // extend this gap left side
            for (int x=1; x < 4 && i > 0; x++) {
                if (at(i-1).state != UNKNOWN || at(i-1).val > empty_threshold)
                    break;
                make_range(GAP, i-1, rng[i].end);
                i -= 1;
            }
            // ensure row before this gap
            r = i - 1;
            for (int j = i - 1; j >= 0 && i - j <= height_max; j--) {
                if (!(rng[j].state == UNKNOWN || rng[j].state == FLAT))
                    break;
                r = j;
            }
            if (r < 0)
                r = 0;
            if (i - r >= height_min && i - r <= height_max) {
                added = true;
                make_range(ROW, r, i-1);
            }
        }
        // skip to the end of this gap
        i = rng[i].end;
        if (at(i+1).state == EMPTY)
            continue;
        if (at(i+1).state == ROW || at(i+1).state == GAP) {
            // already have row after this gap
            i = rng[i+1].end;
            continue;
        }
        // extend this gap right side
        for (int x=1; x < 4 && i < size-1; x++) {
            if (rng[i+1].state != UNKNOWN || rng[i+1].val > empty_threshold)
                break;
            make_range(GAP, rng[i].bgn, i+1);
            i += 1;
        }
        // ensure row after this gap
        r = i + 1;
        for (int j = i + 1; j < size && j - i <= height_max; j++) {
            if (!(rng[j].state == UNKNOWN || rng[j].state == FLAT))
                break;
            r = j;
        }
        if (r >= size)
            r = size - 1;
        if (r - i >= height_min && r - i <= height_max) {
            added = true;
            make_range(ROW, i+1, r);
        }
        i = r;
    }
    return added;
}

bool Redused::normalize_rows() {
    bool changed = false;
    int row_norm = int(std::round(list->row_height*scale));
    for (int i=0; i < size; i++) {
        if (rng[i].state != ROW)
            continue;
        int min_val = rng[i].val - 10;
        int row_bgn = i;
        int row_end = rng[i].end;
        int row_size = row_end - row_bgn + 1;
        if (row_size < row_norm+1) {
            auto& prv = at(row_bgn-1);
            auto& nxt = at(row_end+1);
            bool can_ext_before = (prv.val >= min_val && prv.state == GAP && at(row_bgn-2).state != ROW);
            bool can_ext_after  = (nxt.val >= min_val && nxt.state == GAP && at(row_end+2).state != ROW);
            if (can_ext_before && can_ext_after)
                can_ext_before = (prv.val < nxt.val);
            if (can_ext_after) {
                changed = true;
                make_range(ROW, rng[i].bgn, rng[i].end+1);
            }
            if (can_ext_before) {
                changed = true;
                make_range(ROW, rng[i].bgn-1, rng[i].end);
            }
        }
        i = rng[row_end].end;
    }
    return changed;
}

#define DEBUG_LIST_DETECTOR 1

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
    cv::Rect rowTestRect = listCapturedRect;
    if (row_test_end > row_test_bgn) {
        rowTestRect.x += env.getScale() * row_test_bgn;
        rowTestRect.width = env.getScale() * (row_test_end - row_test_bgn);
    }
    detect::HsvValueCropFilter hsvFilter;
    hsvFilter.rangesU.emplace_back(cv::Vec3b(5,100,30),cv::Vec3b(35,255,255));
    XMat testImage = hsvFilter.apply(env.getColorImage()(rowTestRect), {});
#ifdef  DEBUG_LIST_DETECTOR
    XMat origImage = env.getColorImage()(rowTestRect);
    cv::Mat origImageDebug = toMat(origImage).clone();
    cv::Mat testImageDebug = toMat(testImage).clone();
#endif
    cv::reduce(testImage, reducedImage, 1, cv::REDUCE_AVG, CV_8UC1);
    cv::Mat reducedMat = toMat(reducedImage);
    Redused reduced(this, reducedMat.rows, reducedMat.data, env.getScale());
    reduced.detect_flat();
    reduced.detect_gaps();
    reduced.detect_rows();
    reduced.calc_threshold();
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
        auto& r = reduced.at(i);
        if (r.state == Redused::FLAT)
            histImage.at<uchar>(0, i) = 180;
        if (r.state == Redused::GAP)
            histImage.at<uchar>(2, i) = r.artificial ? 120 : 180;
        else if (r.state == Redused::ROW)
            histImage.at<uchar>(4, i) = r.artificial ? 120 : 255;
        histImage.at<uchar>(r.val, i) = 128;
        histImage.at<uchar>(reduced.pix[i], i) = 255;
    }
#endif

    bool has_rows = false;
    for (int i=0; i < reduced.size; i++) {
        if (reduced.rng[i].state != Redused::ROW) {
            i = reduced.rng[i].end;
            continue;
        }
        if (reduced.rng[i].val < reduced.empty_threshold) {
            i = reduced.rng[i].end;
            continue;
        }
        int row_bgn = reduced.rng[i].bgn;
        int row_end = reduced.rng[i].end;
        int row_val = reduced.rng[i].val;
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
            return ReferenceScreenSize.width;
        if (equalsIgnoreCase(view, "ScreenHeight"))
            return ReferenceScreenSize.height;
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
    for (int i=0; i < 2; i++) {
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
