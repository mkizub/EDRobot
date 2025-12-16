//
// Created by mkizub on 02.07.2025.
//
#include "../pch.h"

#include "Detector.h"
#include "Lines.h"

#include <opencv2/ximgproc.hpp>

#include <iomanip>

namespace detect {

LineDetector::LineDetector(spEvalLine line)
        : referenceLine(line)
        , extendAngleMin(-5.f)
        , extendAngleMax(+5.f)
        , angleStep(1.f)
        , houghThreshold(0)
{
}

static float distanceToLine(cv::Point2f p, cv::Line2f l) {
    // (y2-y1)*x0 - (x2-x1)*y0 + x2*y1 - y2*x1 / sqrt((y2-y1)^2 + (x2-x1)^2)
    double dist = (l.p1().y-l.p0().y)*p.x - (l.p1().x-l.p0().x)*p.y + l.p1().x*l.p0().y - l.p1().y*l.p0().x;
    double len = cv::norm(l.p1() - l.p0());
    return float(dist / len);
}

#define LINE_THR 200
bool isLineNoise(const cv::Mat& lineImage, int pos, bool start) {
    for (int x=pos+1, cnt=0; cnt < 7 && x < lineImage.cols; x++, cnt++) {
        int sum = 0;
        for (int y=0; y < lineImage.rows; y++)
            sum += lineImage.at<uchar>(y,x);
        if ((sum < LINE_THR) == start)
            return true;
    }
    return false;
}

void extractLine(const cv::Mat& lineImage, int& y0, int& x0, int& x1, int minWidth, int maxWidth) {
    y0 = 0;
    x0 = -1;
    x1 = -1;
    int h_sum[10] {};
    for (int x=0; x < lineImage.cols; x++) {
        if (x-x0 > maxWidth)
            break;
        int v_sum = 0;
        for (int y=0; y < lineImage.rows; y++) {
            int v = lineImage.at<uchar>(y, x);
            v_sum += v;
            if (x0 >= 0)
                h_sum[y] += v;
        }
        if (v_sum >= LINE_THR) {
            if (x0 < 0) {
                if (!isLineNoise(lineImage, x, true)) {
                    x0 = x;
                    x += minWidth;
                }
                continue;
            } else {
                if (!isLineNoise(lineImage, x, false))
                    x1 = x;
            }
        }
    }
    int h_sum_max = -1;
    for (int y=0; y < lineImage.rows; y++) {
        if (h_sum[y] > h_sum_max) {
            y0 = y;
            h_sum_max = h_sum[y];
        }
    }
}

double LineDetector::match(ClassifyEnv &env) {
    cv::Line refLine = referenceLine->calcReferenceLine(env);
    return match(env, refLine, XMat());
}

double LineDetector::match(ClassifyEnv& env, cv::Line refLine, XMat gameImage) {
    detectedLines.clear();
    expectedLine = env.cvtReferenceToCaptured(refLine);
    cv::Point captureP0 = expectedLine.p0();
    cv::Point captureP1 = expectedLine.p1();
    const auto captureWidth = expectedLine.length();
    cv::Rect r0 = cv::Rect(captureP0 - env.scaleToCaptured(extendLT),
                           captureP0 + env.scaleToCaptured(extendRB));
    cv::Rect r1 = cv::Rect(captureP1 - env.scaleToCaptured(extendLT),
                           captureP1 + env.scaleToCaptured(extendRB));
    lineMatchRect = r0 | r1;
    env.cropToCapture(lineMatchRect);
    cv::Point2f matchCenter = (lineMatchRect.tl() + lineMatchRect.br()) * 0.5f;
    lastAvrgAngle = expectedLine.angle();
    lastDeltaAngle = 0;

    if (gameImage.empty())
        gameImage = env.getColorImage();
    XMat imagePrepared = ImageTemplate::applyFilters(filters, gameImage(lineMatchRect));
    //cv::threshold(imagePrepared, imagePrepared, 127, 255, cv::THRESH_BINARY);

    double referenceAngle = refLine.angle();

    int threshold;
    if (houghThreshold > 0)
        threshold = houghThreshold * env.getScale();
    else
        threshold = captureWidth * 0.95;
    double expAngle = 90+referenceAngle;
    {
#ifdef EDROBOT_USE_OPENCL
        XMat linesX;
        cv::HoughLines(imagePrepared, linesX, 1, angleStep * CV_PI / 180, threshold, 0, 0, 0, CV_PI);
        if (linesX.rows <= 0)
            return 0;
        cv::Mat lines = toMat(linesX);
#else
        cv::Mat lines;
        cv::HoughLines(imagePrepared, lines, 1, angleStep * CV_PI / 180, threshold, 0, 0,
                (expAngle - extendAngleMin) * CV_PI / 180, (expAngle + extendAngleMax) * CV_PI / 180
        );
#endif
        //LOG(DEBUG) << "LineDetector '" << name << "' found " << lines.rows << " lines";
        for (int ln = 0; ln < lines.rows; ln++) {
            auto &lv = lines.at<cv::Vec2f>(ln);
            float rho = lv[0];
            float angle = lv[1]; // radians, perpendicular to line
            float angleDeg = angle * 180 / CV_PI; // degree of line perpendicular
#ifdef EDROBOT_USE_OPENCL
            if (angleDeg < (expAngle - extendAngleMin) || angleDeg > (expAngle + extendAngleMax))
                continue;
#endif
            double cos_a = cos(angle);
            double sin_a = sin(angle);
            double x0 = cos_a * rho;
            double y0 = sin_a * rho;
            cv::Point pt1{cvRound(x0 + 100 * (-sin_a)), cvRound(y0 + 100 * (cos_a))};
            cv::Point pt2{cvRound(x0 - 100 * (-sin_a)), cvRound(y0 - 100 * (cos_a))};
            float dist_to_center = distanceToLine(matchCenter, cv::Line2d(pt1, pt2));
            angleDeg -= 90; // degree of line
            detectedLines.emplace_back(rho, angleDeg, dist_to_center, 1);
        }
    }

    for (int i=0; i < detectedLines.size(); i++) {
        auto& dl1 = detectedLines[i];
        for (int j=i+1; j < detectedLines.size(); j++) {
            auto& dl2 = detectedLines[j];
            if (std::abs(dl1.angle - dl2.angle) < 2 &&
                (std::abs(dl1.dist_to_center - dl2.dist_to_center) < 3 ||
                 std::abs(dl1.rho - dl2.rho) < 3))
            {
                dl1.rho = (dl1.rho * dl1.count + dl2.rho * dl2.count) / (dl1.count + dl2.count);
                dl1.angle = (dl1.angle * dl1.count + dl2.angle * dl2.count) / (dl1.count + dl2.count);
                dl1.dist_to_center = (dl1.dist_to_center * dl1.count + dl2.dist_to_center * dl2.count) / (dl1.count + dl2.count);
                dl1.count += dl2.count;
                detectedLines.erase(detectedLines.begin() + j);
            }
        }
    }

    int linesFound = 0;
    double sumAngle = 0; // degrees
    for (auto& dl : detectedLines) {
        linesFound += dl.count;
        sumAngle += dl.angle * dl.count;

        double angle = (90+dl.angle)*M_PI/180;
        double cos_a = cos(angle);
        double sin_a = sin(angle);
        double x0 = cos_a * dl.rho;
        double y0 = sin_a * dl.rho;
        // {x,y} = {x0 + P * (-sin_a), y0 + P * (cos_a)}
        // {0,y} = {x0 + P0 * (-sin_a), y0 + P0 * (cos_a)}
        // x0 + P0 * (-sin_a) = 0
        // P0 = x0/sin_a
        // {W,y} = {x0 + PW * (-sin_a), y0 + PW * (cos_a)}
        // x0 + PW * (-sin_a) = W
        // PW = (x0-W)/sin_a
        double P0 = x0 / sin_a;
        double PW = (x0-lineMatchRect.width) / sin_a;
        cv::Point2d pt0 {x0 + P0 * (-sin_a), y0 + P0 * (cos_a)};
        cv::Point2d ptW {x0 + PW * (-sin_a), y0 + PW * (cos_a)};
        dl.line = cv::Line2d(pt0, ptW);
    }
    if (!linesFound)
        return 0;
    lastAvrgAngle = sumAngle / linesFound;
    lastDeltaAngle = lastAvrgAngle - referenceAngle;
    std::sort(detectedLines.begin(), detectedLines.end(), [](auto& l1, auto& l2){
        return l1.rho < l2.rho;
    });

    DetectedLine* detEdge;
    if (detectEdgesMode == +1)
        detEdge = &detectedLines.front();
    else if (detectEdgesMode == -1)
        detEdge = &detectedLines.back();
    else
        detEdge = nullptr;

    if (detEdge) {
        cv::Matx23d affineMatrix = cv::getRotationMatrix2D_(detEdge->line.p0(), detEdge->angle, 1);
        affineMatrix.val[5] -= detEdge->line.p0().y - 2;
        cv::Size lineImageSize{imagePrepared.cols, 5};
        cv::Mat lineImage;
        cv::warpAffine(imagePrepared, lineImage, affineMatrix, lineImageSize, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);

        int y0, x0, x1;
        extractLine(lineImage, y0, x0, x1, int(captureWidth * 0.97), int(captureWidth * 1.03));
        if (env.isDebugMatch())
            LOG(INFO) << "LineDetector found " << linesFound << " lines, x0=" << x0 << " x1=" << x1;
        std::array<cv::Point2f, 2> points{cv::Point2f(x0, y0 - 2), cv::Point2f(x1, y0 - 2)};
        cv::Matx23d invAffineMatrix;
        cv::invertAffineTransform(affineMatrix, invAffineMatrix);
        cv::transform(points, points, invAffineMatrix);
        captureP0 = cv::Point(points[0]) + lineMatchRect.tl();
        captureP1 = cv::Point(points[1]) + lineMatchRect.tl();
        detectedLine = {captureP0, captureP1};
        lastDeltaScale = cv::norm(captureP1 - captureP0) / captureWidth;
    } else {
        detectedLine = {};
        lastDeltaScale = 1;
    }

    ClassifiedRect& cr = env.classified.emplace_back(ClsDetType::LineDetected, false, name + ':', cv::Rect());
    if (!detectedLine.empty()) {
        cr.u.ldet.referenceLine = env.cvtCapturedToReference(detectedLine);
        cr.u.ldet.scale = lastDeltaScale;
        cr.u.ldet.offset = expectedLine.p0()-detectedLine.p0();
    } else {
        cr.u.ldet.referenceLine = {};
        cr.u.ldet.scale = 1;
        cr.u.ldet.offset = {};
    }
    cr.u.ldet.angle = lastDeltaAngle;
    cr.u.ldet.match = 1;
    cr.u.ldet.detector = this;
    return 1;
}

} // detect
