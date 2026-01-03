//
// Created by mkizub on 26.12.2025.
//

#include "../pch.h"

#include "EDWidget.h"
#include "List.h"
#include "../FuzzyMatch.h"
#include "../OCR.h"

namespace widget {

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
    struct Row {
        float bgn;
        float end;
        float len;
        short val;
        short avr;
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
    std::vector<Row> get_rows();
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

// y = a*x + b
struct XY {
    double x;
    double y;
};
static void leastSquare(const std::vector<XY>& data, double* a, double* b) {
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    int n = data.size();
    for (const auto& p : data) { // data is vector<pair<double, double>>
        sum_x += p.x;
        sum_y += p.y;
        sum_xy += p.x * p.y;
        sum_x2 += p.x * p.x;
    }
    // Calculate m and c using formulas (derived from normal equations)
    *a = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    *b = (sum_y - *a * sum_x) / n;
}

std::vector<Redused::Row> Redused::get_rows() {
    std::vector<std::pair<int,double>> gaps;
    double drow = list->row_height * scale;
    double dgap = list->row_gap * scale;
    double row_plus_gap = drow + dgap;
    for (int i=0; i < size; i++) {
        if (rng[i].state == GAP) {
            auto& r = rng[i];
            double sum_w = 0;
            double sum_p = 0;
            double max_v = r.max * 1.2;
            double mid = (r.bgn+r.end) * 0.5;
            if (max_v > 0) {
                for (int p = r.bgn; p <= r.end; p++) {
                    double w = (max_v - pix[p]) / max_v;
                    sum_w += w;
                    sum_p += p * w;
                }
                mid = sum_p / sum_w;
            }
            gaps.emplace_back(i, mid);
        }
        i = rng[i].end;
    }
    while (gaps.size() > 2 && std::abs(gaps[1].second-gaps[0].second - row_plus_gap) > 2*dgap)
        gaps.erase(gaps.begin());
    while (gaps.size() > 2 && std::abs(gaps[gaps.size()-1].second-gaps[gaps.size()-2].second - row_plus_gap) > 2*dgap)
        gaps.pop_back();

    std::vector<double> offsets1;
    std::vector<double> offsets2;
    cv::Scalar mean_val, stddev_val;
    gaps.reserve(30);
    offsets1.reserve(30);
    offsets2.reserve(30);
    for (auto g : gaps) {
        assert (rng[g.first].state == GAP);
        if (rng[g.first].state == GAP) {
            double off1 = std::fmod(g.second, row_plus_gap);
            double off2 = std::fmod(g.second+row_plus_gap*0.5, row_plus_gap) - row_plus_gap*0.5;
            offsets1.push_back(off1);
            offsets2.push_back(off2);
        }
    }

    cv::meanStdDev(offsets1, mean_val, stddev_val);
    double mean = mean_val[0];
    double stddev = stddev_val[0];
    cv::meanStdDev(offsets2, mean_val, stddev_val);
    if (stddev_val[0] < stddev) {
        offsets1 = offsets2;
        mean = mean_val[0];
        stddev = stddev_val[0];
    }
    //LOG(INFO) << "list mean offs: " << mean;

    std::vector<XY> leastApproxData;
    leastApproxData.reserve(gaps.size());
    for (int i=0; i < offsets1.size(); i++) {
        leastApproxData.emplace_back(gaps[i].second,offsets1[i]-mean);
    }
    double A, B;
    leastSquare(leastApproxData, &A, &B);

    std::vector<Row> rows;
    double y_end = size-drow+dgap;
    bool prev_was_empty = false;
    for (double y = mean+dgap*0.5; y <= y_end; y += row_plus_gap) {
        double off = y*A + B;
        if (y + off + dgap < 0)
            continue;
        double rb = std::clamp(y+off, 0.0, size - 1.0);
        double re = std::clamp(y+off+drow, 0.0, size - 1.0);
        int b = (int) std::round(rb);
        int e = (int) std::round(re);

        int sum = pix[b+1];
        uint8_t min = pix[b+1];
        uint8_t max = pix[b+1];
        for (int i=b+2; i < e; i++) {
            auto pv = pix[i];
            sum += pv;
            min = std::min(min, pv);
            max = std::max(max, pv);
        }
        uint8_t avr = sum / (e - b - 1);
        uint8_t val = min;
        if (avr > min+4)
            val = avr - 4;

        if (val < empty_threshold /*&& list->header > 0*/) {
            if (prev_was_empty)
                break;
            prev_was_empty = true;
            continue;
        }
        prev_was_empty = false;
        rows.emplace_back(float(rb), float(re), float(re-rb), val, avr);
    }
    return rows;
}


//#define DEBUG_LIST_DETECTOR 1
#if defined(DEBUG_LIST_DETECTOR) && defined(NDEBUG)
# error "DEBUG_LIST_DETECTOR in release build"
#endif

bool List::detect(DetectParams& params) {
    ClassifyEnv& env = params.env;
    cv::Rect listReferenceRect = env.calcReferenceRect(this->rect);
    cv::Rect listCapturedRect =  env.cvtReferenceToCaptured(listReferenceRect);
    env.cropToCapture(listCapturedRect);
    if (listCapturedRect.empty())
        return false;

    env.classified.emplace_back(ClsDetType::Widget, this->name, listReferenceRect);
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
    //for (bool added = true; added;) {
    //    added = reduced.insert_gaps();
    //    added |= reduced.insert_rows();
    //}
    //while (reduced.normalize_rows())
    //    ;
    auto rows = reduced.get_rows();
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
    for (auto& r : rows) {
        for (int p=r.bgn; p <= r.end; p++)
            histImage.at<uchar>(6, p) = 255;
    }
#endif

    bool has_rows = false;
    for (auto& r : rows) {
        has_rows = true;

        cv::Rect2f rowCapturedRect {0.f, r.bgn, (float)listCapturedRect.width, r.end-r.bgn+1};
        rowCapturedRect += cv::Point2f(listCapturedRect.tl());
        cv::Rect rowReferenceRect = env.cvtCapturedToReference(rowCapturedRect);

        WState ws = WState::Unknown;
        if (r.val > 120) // bright = focused
            ws = WState::Focused;
        else
            ws = WState::Normal;

#ifdef  DEBUG_LIST_DETECTOR
        cv::Mat rowImage = toMat(env.getColorImage()(rowCapturedRect));
#endif
        env.classified.emplace_back(ClsDetType::ListRow, "", rowReferenceRect);
        ClassifiedRect& clsRowRect = env.classified.back();
        clsRowRect.u.lrow.capturedRect = rowCapturedRect;
        clsRowRect.u.lrow.list = this;
        clsRowRect.u.lrow.ws = ws;
        clsRowRect.u.lrow.text_confidence = -1;
        if (ws == WState::Focused) {
            clsListRect.u.widg.ws = WState::Focused;
            if (!params.uiState.focused)
                params.uiState.focused = this;
        }
    }
    return has_rows;
}

const List::Tab& List::getTab(std::string_view name) const {
    for (auto& t : tabs) {
        if (t.name == name)
            return t;
    }
    throw std::out_of_range(name.data());
}

bool ListPPC::detect(widget::Widget::DetectParams &params) {
    ClassifyEnv& env = params.env;
    cv::Rect listReferenceRect = env.calcReferenceRect(this->rect);
    cv::Rect listCapturedRect =  env.cvtReferenceToCaptured(listReferenceRect);
    env.cropToCapture(listCapturedRect);
    if (listCapturedRect.empty())
        return false;

#ifdef DEBUG_LIST_DETECTOR
    cv::Mat debugImage = toMat(env.getColorImage()).clone();
    cv::rectangle(debugImage, listCapturedRect, {96,96,96});
#endif

    auto crList = env.classified.emplace_back(ClsDetType::Widget, this->name, listReferenceRect);
    ClassifiedRect& clsListRect = env.classified.back();
    clsListRect.u.widg.referenceRect = listReferenceRect;
    clsListRect.u.widg.ws = WState::Unknown;
    clsListRect.u.widg.widget = this;

    const Tab& tab_icon = getTab("icon");
    const Tab& tab_name = getTab("name");

    if (!icon_detector) {
        icon_detector = std::make_unique<detect::ImageTemplate>("templates/contacts/pp-list-icon.png", nullptr);
        icon_detector->extendLT = {10,10};
        icon_detector->extendRB = {10,10};
    }

    FuzzyMatch fm;
    std::vector<Commodity*> resources;
    for (auto* c : Cfg.getAllKnownCommodities()) {
        if (c->category && c->category->intId == 16) {
            resources.push_back(c);
        }
    }
    cv::Rect refRow = cv::Rect(0,0,listReferenceRect.width,row_height);
    refRow += listReferenceRect.tl();
    for (int i=0; i < 10; i++) {
        cv::Rect iconRect{tab_icon.tab_left + listReferenceRect.x, refRow.y,
                          tab_icon.tab_right - tab_icon.tab_left, (int) row_height};
#ifdef DEBUG_LIST_DETECTOR
        cv::rectangle(debugImage, env.cvtReferenceToCaptured(iconRect), {96,96,96});
#endif
        icon_detector->withRefRect = iconRect;
        double match = icon_detector->match(env);
        if (match < 0.5)
            break;
#ifdef DEBUG_LIST_DETECTOR
        cv::rectangle(debugImage, icon_detector->captureRect, {128,128,128});
#endif
        cv::Point captCenter(listCapturedRect.x, icon_detector->captureRect.y + icon_detector->captureRect.height/2);
        double captHalfHeight = row_height * env.getScale() * 0.5;
        refRow.y = env.cvtCapturedToReference(captCenter).y - refRow.height/2;
        if (i == 0)
            crList.detectedRect.y = refRow.y;
        crList.detectedRect.height = refRow.y + refRow.height - crList.detectedRect.y;

        cv::Rect rowTestRect (refRow.x+row_test_bgn, refRow.y, row_test_end - row_test_bgn, refRow.height);
        rowTestRect = env.cvtReferenceToCaptured(rowTestRect);
#ifdef DEBUG_LIST_DETECTOR
        cv::rectangle(debugImage, rowTestRect, {96,96,96});
        cv::line(debugImage, captCenter, captCenter+cv::Point(100,0), {96,96,96});
#endif
        detect::Histogram hist(detect::Histogram::Mode::Hsv);
        XMat testImage = env.getColorImage()(rowTestRect);
        auto ws = hist.guessWState(testImage);
        Commodity* commodity = nullptr;
        int ocr_conf = 0;
        if (ws != WState::Disabled) {
            double ocr_height = env.getScale() * tab_name.ocr_height * ocr::LINE_HEIGHT / (ocr::ASCENT+ocr::DESCENT);
            cv::Rect nameRect{ listCapturedRect.x + int(tab_name.tab_left*env.getScale()),
                               int(captCenter.y - captHalfHeight + 5*env.getScale()),
                               int((tab_name.tab_right - tab_name.tab_left)*env.getScale()),
                               int(captHalfHeight * 2 - 3*env.getScale()) };
            cv::Mat grayImage;
            cv::cvtColor(env.getColorImage()(nameRect), grayImage, cv::COLOR_BGR2GRAY);
            std::string text;
            ocr_conf = ocr::ocrDetectorText(ocr::GENERIC, ocr_height, true, grayImage, env, text, nullptr);
            std::wstring wtext = toUtf16(text);

            double bestRate = 0;
            for (auto &res: resources) {
                std::wstring wt = wtext.substr(0, res->wocr.size());
                double rate = fm.ratio(res->wocr, wt);
                if (rate > bestRate) {
                    bestRate = rate;
                    commodity = res;
                }
            }
            LOG(INFO) << "PP resource: " << (commodity ? commodity->nameId : "?");
        } else {
            LOG(INFO) << "PP resource: disabled";
        }

        env.classified.emplace_back(ClsDetType::ListRow, "", refRow);
        ClassifiedRect& clsRowRect = env.classified.back();
        clsRowRect.u.lrow.capturedRect = env.cvtReferenceToCaptured(refRow);
        clsRowRect.u.lrow.list = this;
        clsRowRect.u.lrow.ws = ws;
        clsRowRect.u.lrow.commodity = commodity;
        clsRowRect.u.lrow.text_confidence = ocr_conf;

        refRow.y += row_height + row_gap;
    }

    return true;
}

} // namespace widget
