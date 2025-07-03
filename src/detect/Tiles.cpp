//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"

#include <iomanip>

namespace detect {

TilesDetector::TilesDetector(const std::string& name, cv::Rect& tilesRect,
                             const std::string& icons, cv::Rect& iconsRect,
                             int rows_min, int rows_max, int cols_min, int cols_max, int gap)
        : ImageTemplate(icons, iconsRect)
        , name(name)
        , mTilesRect(tilesRect)
        , mMinRows(rows_min)
        , mMaxRows(rows_max)
        , mMinCols(cols_min)
        , mMaxCols(cols_max)
        , mGap(gap)
{
   testScales.push_back(1);
   testAngles.push_back(0);
   channels = 1; // force grayscale
}

bool TilesDetector::getColSpan(int &out_col, int &out_span, cv::Rect &bbox, cv::Rect &captureRect, int gap) const {
    int best_col = -1;
    int best_span = -1;
    int best_col_dist = 1000;
    for (int numCols = mMinCols; numCols <= mMaxCols; numCols++) {
        int col = -1;
        int span = -1;
        int dist = 0;
        for (int i = 0; i <= numCols; i++) {
            int x_col = i * captureRect.width / numCols;
            if (bbox.x >= x_col - gap && bbox.x <= x_col + gap) {
                col = i;
                dist += std::abs(bbox.x - x_col);
            }
            if ((bbox.x + bbox.width) >= x_col - gap && (bbox.x + bbox.width) <= x_col + gap) {
                span = i - col;
                dist += std::abs((bbox.x + bbox.width) - x_col);
            }
        }
        if (col < 0 || span < 0)
            continue;
        if (dist < best_col_dist) {
            best_col_dist = dist;
            best_col = col;
            best_span = span;
        }
    }
    if (best_col <0 || best_span <= 0)
        return false;
    out_col = best_col;
    out_span = best_span;
    return true;
}

double TilesDetector::match(ClassifyEnv &env) {
    cv::Rect captureRect = env.cvtReferenceToCaptured(mTilesRect);
    cv::Mat roiImage(env.getGrayImage(), captureRect);
    if (roiImage.empty())
        return 0;

    unsigned buttonGrayColor;
    if (hudTryHard) {
        int histSize = 256;
        float range[]{0, 256}; //the upper boundary is exclusive
        const float *histRange[]{range};
        cv::Mat hist;
        cv::calcHist(&roiImage, 1, nullptr, cv::Mat(), hist, 1, &histSize, histRange);
        std::vector<float> hv;
        for (int i = 0; i < hist.total(); ++i)
            hv.push_back(hist.at<float>(i));
        int maxLoc[4]{};
        cv::minMaxIdx(hist, nullptr, nullptr, nullptr, maxLoc);
        buttonGrayColor = maxLoc[0] - 4;
    } else {
        buttonGrayColor = Master::getInstance().getConfiguration()->getButtonGrayColor(WState::Normal);
    }

    cv::Mat thrImage;
    cv::threshold(roiImage, thrImage, buttonGrayColor - 2, 255, cv::THRESH_BINARY);

    if (hudTryHard) {
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
        cv::Mat erodedImage;
        cv::erode(thrImage, erodedImage, kernel, cv::Point(-1, -1), 2, cv::BORDER_CONSTANT, cv::Scalar::all(0));
        cv::threshold(erodedImage, thrImage, 250, 255, cv::THRESH_BINARY);
    }

    int hGaps = (mMaxCols - 1) * mGap * env.getScale();
    int minTileWidth = (captureRect.width - hGaps) / mMaxCols - mGap;
    int vGaps = (mMaxRows - 1) * mGap * env.getScale();
    int minTileHeight = (captureRect.height - vGaps) / mMaxRows - mGap;
    int minTileArea = minTileWidth * minTileHeight;
    int maxTileArea = captureRect.area() - std::min(minTileArea*mMinCols,minTileArea*mMinRows);

    mDetectedTiles.clear();
    std::vector<std::vector<cv::Point>> contours;
    cv::findContoursLinkRuns(thrImage, contours);
    for (const auto &contour: contours) {
        std::vector<cv::Point> convex;
        cv::convexHull(contour, convex);
        if (convex.size() >= 4) {
            std::vector<cv::Point> approx;
            cv::approxPolyN(convex, approx, 4, 5, true);
            cv::Rect bbox = cv::boundingRect(approx);
            if (bbox.width >= minTileWidth && bbox.height >= minTileHeight &&
                bbox.area() >= minTileArea && bbox.area() <= maxTileArea) {
                int col, span;
                if (!getColSpan(col, span, bbox, captureRect, int(mGap * env.getScale())))
                    continue;
                bbox += captureRect.tl();
                bbox &= captureRect;
                cv::Rect refRect = env.cvtCapturedToReference(bbox);
                mDetectedTiles.emplace_back(ClsDetType::Tile, env.isWarpMode(), name + ":", refRect);
                mDetectedTiles.back().u.tile.row = -1;
                mDetectedTiles.back().u.tile.col = col;
                mDetectedTiles.back().u.tile.span = span;
            }
        }
    }
    LOG(DEBUG) << "TilesDetector '" << name << "' found " << mDetectedTiles.size() << " tiles";

    int area = 0;
    for (auto &cr: mDetectedTiles)
        area += env.scaleToCaptured(cr.detectedRect.size()).area();
    if (area < captureRect.area() * 0.8)
        return 0;

    for (int c = 0; c < mMaxCols; c++) {
        std::vector<ClassifiedRect *> colSet;
        for (auto &cr: mDetectedTiles) {
            if (cr.u.tile.col <= c && cr.u.tile.col + cr.u.tile.span > c)
                colSet.push_back(&cr);
        }
        std::sort(colSet.begin(), colSet.end(), [](ClassifiedRect *c1, ClassifiedRect *c2) {
            return c1->detectedRect.y < c2->detectedRect.y;
        });
        int row = 0;
        for (ClassifiedRect *cr: colSet) {
            if (cr->u.tile.row > row)
                row = cr->u.tile.row + 1;
            else
                cr->u.tile.row = row++;
        }
    }

    prepareImages(env);

    for (auto &cr: mDetectedTiles) {
        cv::Rect tileRect = env.cvtReferenceToCaptured(cr.detectedRect);
        tileRect &= captureRect;
        tileRect -= captureRect.tl();
        ImageMatrix *bestIcon = nullptr;
        double bestIconVal = 0;
        for (auto &ic: imagesPrepared) {
            int result_cols = tileRect.width - ic.templImage.cols + 1;
            int result_rows = tileRect.height - ic.templImage.rows + 1;
            if (result_cols <= 0 || result_rows <= 0)
                continue;
            cv::Mat result(result_rows, result_cols, CV_32FC1);
            cv::Mat tileImage = cv::Mat(roiImage, tileRect);
            cv::matchTemplate(tileImage, ic.templImage, result, cv::TM_CCOEFF_NORMED);
            //LOG(ERROR) << "match result: " << result;
            double maxVal;
            cv::Point maxLoc;
            cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
            //LOG(DEBUG) << "match result: " << std::setprecision(3) << maxVal << " for " << ic.name;
            if (maxVal >= threshold_min && maxVal > bestIconVal) {
                bestIcon = &ic;
                bestIconVal = maxVal;
            }
        }
        if (!name.empty() && bestIcon && bestIconVal >= threshold_min) {
            cr.text = name + ":" + bestIcon->name;
            LOG(DEBUG) << "TilesDetector matched result: " << std::setprecision(3) << bestIconVal
                       << " for " << cr.text
                       << " row:" << cr.u.tile.row << " col:" << cr.u.tile.col << " span:" << cr.u.tile.span;
            env.classified.push_back(cr);
        } else {
            LOG(DEBUG) << "TilesDetector matched failed: " << std::setprecision(3) << bestIconVal
                       << " for " << (bestIcon ? bestIcon->name : "all")
                       << " row:" << cr.u.tile.row << " col:" << cr.u.tile.col << " span:" << cr.u.tile.span
                       << " rect " << cr.detectedRect;
            env.classified.push_back(cr);
        }
    }
    return 1;
}

double TilesDetector::classify(ClassifyEnv &env) {
    return match(env);
}

double TilesDetector::debugMatch(ClassifyEnv &env) {
    //CompassDetector::tryLowerUpperBoundsGUI(env, mRect->calcReferenceRect(env));
    double value = match(env);
    LOG(INFO) << " detected " << mDetectedTiles.size() << " tiles:";
    for (auto &cr: env.classified) {
        if (cr.cdt == ClsDetType::Tile && cr.text.starts_with(name + ":")) {
            LOG(INFO) << "   tile: '" << cr.text << "' rect: " << cr.detectedRect
                      << " col: " << cr.u.tile.col << " row: " << cr.u.tile.row;
        }
    }
    cv::Scalar colorOk(96, 255, 255);
    cv::Scalar colorNo(96, 96, 255);
    cv::Rect captureRect = env.cvtReferenceToCaptured(mTilesRect);
    cv::rectangle(env.getDebugImage(), captureRect.tl(), captureRect.br(), (value < 0.5 ? colorNo : colorOk), 1);
    for (auto &cr: mDetectedTiles) {
        cv::Rect r = env.cvtReferenceToCaptured(cr.detectedRect);
        cv::Scalar color = cr.text.size() > name.size() + 1 ? colorOk : colorNo;
        cv::rectangle(env.getDebugImage(), r.tl(), r.br(), color, 1);
    }
    return value;
}

} // detect
