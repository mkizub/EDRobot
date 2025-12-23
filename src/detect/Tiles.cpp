//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"
#include "Tiles.h"
#include "../OCR.h"
#include "../FuzzyMatch.h"

#include <iomanip>

namespace detect {

TilesDetector::TilesDetector(const std::string& name, cv::Rect& tilesRect, cv::Rect& marksRect,
                             int rows_min, int rows_max, int cols_min, int cols_max, int gap)
        : name(name)
        , mTilesRect(tilesRect)
        , mMarksRect(marksRect)
        , mMinRows(rows_min)
        , mMaxRows(rows_max)
        , mMinCols(cols_min)
        , mMaxCols(cols_max)
        , mGap(gap)
{
}

double TilesDetector::match(ClassifyEnv &env) {
    cv::Rect captureRect = env.cvtReferenceToCaptured(mTilesRect);
    XMat roiColorImage = env.getColorImage()(captureRect);
    ChannelFilter grayFilter(ChannelFilter::gray);
    XMat roiGrayImage = grayFilter.apply(roiColorImage,{});
    if (roiGrayImage.empty())
        return 0;

    int gap = mGap * env.getScale();
    std::vector<cv::Rect> tiles = detectColumns(roiGrayImage, gap);
    if (tiles.empty())
        return 0;

    if (name.empty())
        return 1;
    if (name.empty())
        return 1;

    if (icons_detector) {
        if (icons_detector->refSize.empty())
            return 1;

        for (auto &tile: tiles) {
            cv::Point offset;
            icons_detector->withRefRect = mMarksRect;
            if (mIconAlign == Center) {
                icons_detector->withRefRect += cv::Point(tile.size() - mMarksRect.size()) / 2;
            }
            icons_detector->withRefRect += tile.tl() + captureRect.tl();
            double match = icons_detector->match(env);
            auto& ir = icons_detector->lastMatchResult;
            cv::Rect detectedRect = env.cvtCapturedToReference(tile + ir.loc);
            if (match > 0.1) {
                cv::Rect testRect = mTestRect.empty() ? tile : (mTestRect+tile.tl());
                detect::Histogram hist(detect::Histogram::Mode::Hsv);
                WState ws = hist.guessWState(roiColorImage(testRect));
                env.classified.emplace_back(ClsDetType::Tile, name + ":" + ir.im->name, detectedRect);
                env.classified.back().u.tile.capturedRect = tile + ir.loc;
                env.classified.back().u.tile.ws = ws;
                LOG(DEBUG) << std::format("TilesDetector matched result: {:.3f} for {} rect [{}:{},{}x{}]",
                                          ir.value, ir.im->name,
                                          detectedRect.x, detectedRect.y, detectedRect.width, detectedRect.height);
            } else {
                LOG(DEBUG) << std::format("TilesDetector matched failed: {:.3f} for {} rect [{}:{},{}x{}]",
                                          ir.value, icons_detector->filename,
                                          tile.x, tile.y, tile.width, tile.height);
            }
        }
    }
    else if (!labels.empty()) {
        FuzzyMatch fm;
        for (auto &tile: tiles) {
            cv::Rect testRect = mTestRect.empty() ? tile : (mTestRect+tile.tl());
            detect::Histogram hist(detect::Histogram::Mode::Hsv);
            WState ws = hist.guessWState(roiColorImage(testRect));
            cv::Mat ocrImage = toMat(roiGrayImage(mMarksRect+tile.tl()));
            std::string text;
            if (ocr::ocrTileLblText(ocrImage, ws, text) < 50)
                continue;
            std::wstring wtext = toUtf16(text);
            double bestRatio = 0;
            const std::string* bestLabel = nullptr;
            for (auto& lbl : labels) {
                for (auto& t : lbl.second) {
                    double r = fm.ratio(wtext, t);
                    if (r > 0.5 && r > bestRatio) {
                        bestRatio = r;
                        bestLabel = &lbl.first;
                    }
                }
            }
            cv::Rect detectedRect = env.cvtCapturedToReference(tile + captureRect.tl());
            if (bestLabel) {
                env.classified.emplace_back(ClsDetType::Tile, name + ":" + *bestLabel, detectedRect);
                env.classified.back().u.tile.capturedRect = tile;
                env.classified.back().u.tile.ws = ws;
                LOG(DEBUG) << std::format("TilesDetector matched result: {}% for {} rect [{}:{},{}x{}]",
                                          int(bestRatio), *bestLabel,
                                          detectedRect.x, detectedRect.y, detectedRect.width, detectedRect.height);
            } else {
                LOG(DEBUG) << std::format("TilesDetector matched failed: {}% for {} rect [{}:{},{}x{}]",
                                          int(bestRatio), text,
                                          tile.x, tile.y, tile.width, tile.height);
            }
        }
    }
    return 1;
}

std::vector<TilesDetector::Range> TilesDetector::split(bool columns, uchar* reduced, int size, int gap, int& minThreshold) {
    int min_tile_size, max_tile_size;
    if (!mTileSize.empty()) {
        if (columns) {
            min_tile_size = mTileSize.width;
            max_tile_size = mTileSize.width;
        } else {
            min_tile_size = mTileSize.height;
            max_tile_size = mTileSize.height;
        }
    } else {
        if (columns) {
            min_tile_size = (size - gap * (mMaxCols - 1)) / mMaxCols;
            max_tile_size = (size - gap * (mMinCols - 1)) * (mMaxCols - mMinCols + 1) / mMaxCols;
        } else {
            min_tile_size = (size - gap * (mMaxRows - 1)) / mMaxRows * 96 / 100;
            max_tile_size = (size - gap * (mMinRows - 1)) * (mMaxRows - mMinRows + 1) / mMaxRows;
        }
    }
    min_tile_size = min_tile_size * 96 / 100;
    max_tile_size = max_tile_size * 104 / 100;
    int threshold = 35;
    {
        int min_count, max_count;
        if (columns) {
            min_count = 0.8 * (mMinCols-1) * gap;
            max_count = 1.2 * (mMaxCols+1) * gap;
        } else {
            min_count = 0.8 * (mMinRows-1) * gap;
            max_count = 1.2 * (mMaxRows+1) * gap;
        }
        const int histBin = 4;
        const int histSize = 256 / histBin;
        int valHist[histSize] = {};
        for (int i = 0; i < size; i++)
            valHist[reduced[i] / histBin] += 1;
        int min_thr = -1;
        int max_thr = -1;
        int count = 0;
        for (int i = 0; i < histSize; i++) {
            count += valHist[i];
            if (min_thr < 0 && count >= min_count) {
                min_thr = i;
                count = 0;
            }
            if (max_thr < 0 && count >= max_count && i*histBin >= 20) {
                max_thr = i;
                break;
            }
        }
        threshold = histBin * (min_thr + 0.8*(max_thr-min_thr));
        if (threshold < minThreshold)
            threshold = minThreshold;
        else
            minThreshold = threshold;
    }
    //cv::Mat debugImage(256, size, CV_8UC1, cv::Scalar(0));
    bool in_gap = true;
    uchar gap_value = 0;
    struct Gap {
        int bgn, min, end, val;
    };
    std::vector<Gap> detectedGaps;
    detectedGaps.emplace_back(0,0,0, gap_value);
    for (int i = 0; i < size; i++) {
        if (reduced[i] < threshold) {
            if (!in_gap) {
                in_gap = true;
                gap_value = reduced[i];
                detectedGaps.emplace_back(i,i,i, gap_value);
            }
            else if (gap_value < reduced[i]) {
                gap_value = reduced[i];
                detectedGaps.back().min = i;
                detectedGaps.back().val = gap_value;
                detectedGaps.back().end = i;
            } else {
                detectedGaps.back().end = i;
            }
        }
        else if (in_gap) {
            in_gap = false;
        }
        //if (i > 0) {
        //    cv::line(debugImage,
        //             cv::Point(i - 1, reduced[i - 1]),
        //             cv::Point(i, reduced[i]),
        //             cv::Scalar(255));
        //}
    }
    if (!in_gap)
        detectedGaps.emplace_back(size-1,size-1,size-1, 0);
    for (int g=1; g < detectedGaps.size()-1; g++) {
        int b = detectedGaps[g].bgn;
        int e = detectedGaps[g].end;
        if ((e - b) < gap*0.5 || (e - b) > gap*1.5) {
            detectedGaps.erase(detectedGaps.begin()+g);
        }
    }

    std::vector<Range> ranges;
    for (int g=1; g < detectedGaps.size(); g++) {
        int b = detectedGaps[g-1].end;
        int e = detectedGaps[g].bgn;
        if ((e-b) < min_tile_size || (e-b) > max_tile_size)
            continue;
        int v = 0;
        for (int y=b; y < e; y++)
            v += reduced[y];
        v /= e - b;
        ranges.emplace_back(b, e, v);
    }
    for (int r=1; r < ranges.size(); r++) {
        int& gap_beg = ranges[r-1].end;
        int& gap_end = ranges[r].bgn;
        if (gap_beg-gap_end != gap) {
            int mid = (gap_beg+gap_end) / 2;
            gap_beg = mid - gap/2;
            gap_end = gap_beg + gap;
        }
    }
    //for (auto& r : ranges) {
    //    cv::line(debugImage, cv::Point(r.bgn, 0), cv::Point(r.bgn, r.val), cv::Scalar(255));
    //    cv::line(debugImage, cv::Point(r.end, 0), cv::Point(r.end, r.val), cv::Scalar(255));
    //}
    return ranges;
}

std::vector<cv::Rect> TilesDetector::detectColumns(XMat& roiImage, int gap) {
    const int W = roiImage.cols;
    const int H = roiImage.rows;
    int colThreshold = -1;
    std::vector<Range> columns;
    {
        cv::Rect reduceRect(0, 0, W, H / 3); // cut 1/3 in the middle
        XMat reducedImage;
        cv::reduce(roiImage(reduceRect), reducedImage, 0, cv::REDUCE_AVG, CV_8UC1);
        cv::Mat reducedMat = toMat(reducedImage);
        columns = split(true, reducedMat.data, W, gap, colThreshold);
        if (columns.size() < mMinCols || columns.size() > mMaxCols)
            return {};
    }

    std::vector<cv::Rect> tiles;
    for (auto& col : columns) {
        std::vector<Range> rows;
        if (mMaxRows > 1) {
            cv::Rect reduceRect(col.bgn, 0, col.end-col.bgn, H);
            XMat subImage = roiImage(reduceRect);
            XMat reducedImage;
            cv::reduce(roiImage(reduceRect), reducedImage, 1, cv::REDUCE_AVG, CV_8UC1);
            cv::Mat reducedMat = toMat(reducedImage);
            int rowThreshold = -1;
            rows = split(false, reducedMat.data, H, gap, rowThreshold);
        } else {
            rows.emplace_back(0, H - 1, 100);
        }
        for (auto& row : rows) {
            bool merged = false;
            if (mTryMerge) {
                // try merge
                for (int t=0; t < tiles.size(); t++) {
                    auto& tile = tiles[t];
                    if (std::abs(tile.x+tile.width+gap - col.bgn) < gap &&
                        std::abs(row.bgn - tile.y) < gap/2 &&
                        std::abs(row.end - (tile.y+tile.height)) < gap/2
                        )
                    {
                        // left tile found
                        cv::Rect gapRect(tile.x+tile.width, row.bgn, col.bgn-(tile.x+tile.width),row.end-row.bgn);
                        XMat gapImage = roiImage(gapRect);
                        auto mean = cv::mean(gapImage);
                        if (mean.val[0] > colThreshold) {
                            int y = (tile.y + row.bgn) / 2;
                            int h = (tile.height + (row.end-row.bgn)) / 2;
                            tile = cv::Rect(tile.x, y, col.end-tile.x, h);
                            merged = true;
                            break;
                        }
                    }
                }
            }
            if (!merged)
                tiles.emplace_back(col.bgn, row.bgn, col.end-col.bgn, row.end-row.bgn);
        }
    }

    return tiles;
}


} // detect
