//
// Created by mkizub on 20.12.2025.
//

#pragma once

#ifndef EDROBOT_TILES_H
#define EDROBOT_TILES_H

#include "Detector.h"

namespace detect {

class TilesDetector : public Detector {
public:
    TilesDetector(const std::string& name, cv::Rect& tilesRect, cv::Rect& marksRect,
                  int min_rows, int max_rows, int min_cols, int max_cols, int gap);
    ~TilesDetector() override = default;

    double match(ClassifyEnv& env) override;
    std::vector<cv::Rect> detectColumns(ClassifyEnv& env, XMat& roiImage);

    const std::string name;
    std::string icons;
    std::map<std::string,std::vector<std::wstring>> labels;
    cv::Rect mTilesRect;
    cv::Rect mMarksRect;
    cv::Rect mTestRect;
    cv::Size mTileSize;
    double mOcrHeight;
    bool mTryMerge {true};
    std::vector<std::string> mIconFiles;
    enum IconAlign {
        Center, TopLeft
    } mIconAlign;

    std::unique_ptr<ImageTemplate> icons_detector;
    std::vector<ClassifiedRect> mDetectedTiles;

private:
    struct Range {
        int bgn, end, val;
    };
    std::vector<Range> split(ClassifyEnv &env, bool columns, uchar* reduced, int size, int gap, int& threshold);

    int mMinRows;
    int mMaxRows;
    int mMinCols;
    int mMaxCols;
    int mGap;
};

} // namespace detect

#endif //EDROBOT_TILES_H
