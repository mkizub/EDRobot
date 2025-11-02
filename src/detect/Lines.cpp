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

static double distanceToLine(cv::Point2f p, cv::Line2f l) {
    // (y2-y1)*x0 - (x2-x1)*y0 + x2*y1 - y2*x1 / sqrt((y2-y1)^2 + (x2-x1)^2)
    double dist = (l.p1().y-l.p0().y)*p.x - (l.p1().x-l.p0().x)*p.y + l.p1().x*l.p0().y - l.p1().y*l.p0().x;
    double len = cv::norm(l.p1() - l.p0());
    return dist / len;
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

void extractLine(const cv::Mat& lineImage, int& y0, int& x0, int& x1, double minWidth) {
    y0 = 0;
    x0 = -1;
    x1 = -1;
    const int h = lineImage.rows;
    int h_sum[10] {};
    for (int x=0; x < lineImage.cols; x++) {
        int v_sum = 0;
        for (int y=0; y < lineImage.rows; y++) {
            int v = lineImage.at<uchar>(y, x);
            v_sum += v;
            if (x0 >= 0)
                h_sum[y] += v;
        }
        if (v_sum >= LINE_THR) {
            if (x0 < 0 && !isLineNoise(lineImage, x, true))
                x0 = x;
        }
        else if (x0 >= 0 && !isLineNoise(lineImage, x, false)) {
            if ((x - x0) >= minWidth) {
                x1 = x;
                break;
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

AnchoredLineDetector::AnchoredLineDetector(ImageTemplate* anchor, spEvalLine line)
        : LineDetector(line)
{
    anchorDetector.reset(anchor);
}

double AnchoredLineDetector::match(ClassifyEnv &env) {
    ImageTemplate* an = anchorDetector.get();
    if (!an)
        return 0;
    double anchorMatch = an->match(env);
    const cv::Point& extendLT = an->extendLT;
    const cv::Point& extendRB = an->extendRB;

    const cv::Line refLine = referenceLine->calcReferenceLine(env);
    cv::Rect anRefRect(an->refOrig, an->refSize);
    const double expectedAnchorDist = distanceToLine((anRefRect.tl()+anRefRect.br())*0.5, refLine) * env.getScale();
    expectedLine = env.cvtReferenceToCaptured(refLine);
    cv::Point captureP0 = expectedLine.p0() + an->matchedCaptureOffset;
    cv::Point captureP1 = expectedLine.p1() + an->matchedCaptureOffset;
    const int captureWidth = cv::norm(captureP1 - captureP0);
    cv::Rect r0 = cv::Rect(captureP0 - env.scaleToCaptured(extendLT),
                           captureP0 + env.scaleToCaptured(extendRB));
    cv::Rect r1 = cv::Rect(captureP1 - env.scaleToCaptured(extendLT),
                           captureP1 + env.scaleToCaptured(extendRB));
    lineMatchRect = r0 | r1;
    env.cropToCapture(lineMatchRect);

    if (anchorMatch < 0.5 || an->lastTemplatedx < 0) {
        LOG(DEBUG) << "LineDetector: anchor '" << an->filename << "' not found";
        return 0;
    }
    LOG(DEBUG) << "LineDetector '" << name << "' anchor found '" << an->imagesPrepared[an->lastTemplatedx].name << "', offset: " << an->matchedCaptureOffset;

    XMat imagePrepared = ImageTemplate::applyFilters(filters, env.getColorImage()(lineMatchRect));
    //cv::threshold(imagePrepared, imagePrepared, 127, 255, cv::THRESH_BINARY);

    captureAnchor = (an->captureRect.tl() + an->captureRect.br()) * 0.5;
    cv::Point referenceDist = refLine.p1() - refLine.p0();
    double referenceAngle = std::atan2(referenceDist.y, referenceDist.x) * 180 / CV_PI;
    double avrgAngle = 0; // degrees
    double avrgAnchorDist = 0; // pixels
    int linesFound = 0;

    cv::Point2f matchAnchor = captureAnchor - cv::Point2f(lineMatchRect.tl());
    int threshold;
    if (houghThreshold > 0)
        threshold = houghThreshold * env.getScale() * env.getScale();
    else
        threshold = captureWidth * 0.95;
    double expAngle = 90+referenceAngle;
    {
        cv::Mat linesX;
        cv::HoughLines(imagePrepared, linesX, 1, angleStep * CV_PI / 180, threshold, 0, 0,
                       (expAngle + extendAngleMin) * CV_PI / 180, (expAngle + extendAngleMax) * CV_PI / 180);
        //LOG(DEBUG) << "LineDetector '" << name << "' found " << linesX.rows << " lines";
        if (linesX.rows <= 0)
            return 0;
        cv::Mat lines = linesX; //toMat(linesX);
        for (int ln = 0; ln < lines.rows; ln++) {
            auto &lv = lines.at<cv::Vec2f>(ln);
            float rho = lv[0];
            float angle = lv[1]; // radians
            double cos_a = cos(angle);
            double sin_a = sin(angle);
            double x0 = cos_a * rho;
            double y0 = sin_a * rho;
            cv::Point pt1{cvRound(x0 + 100 * (-sin_a)), cvRound(y0 + 100 * (cos_a))};
            cv::Point pt2{cvRound(x0 - 100 * (-sin_a)), cvRound(y0 - 100 * (cos_a))};

            double anchorDist = distanceToLine(matchAnchor, cv::Line2d(pt1, pt2));
            if (std::abs(anchorDist - expectedAnchorDist) < 6 * env.getScale()) { // [-6..+6] reference pixels
                avrgAngle += angle * 180 / CV_PI - 90;
                avrgAnchorDist += anchorDist;
                linesFound += 1;
            }
        }
    }

    if (!linesFound)
        return 0;

    avrgAngle /= linesFound;
    avrgAnchorDist /= linesFound;
    lastLineAngle = avrgAngle;
    lastDeltaAngle = avrgAngle - referenceAngle;

    cv::Matx23d affineMatrix = cv::getRotationMatrix2D_(matchAnchor, avrgAngle, 1);
    affineMatrix.val[5] -= avrgAnchorDist + matchAnchor.y - 2;
    cv::Size lineImageSize {imagePrepared.cols, 5};
    cv::Mat lineImage;
    cv::warpAffine(imagePrepared, lineImage, affineMatrix, lineImageSize, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);

    int y0, x0, x1;
    extractLine(lineImage, y0, x0, x1, captureWidth * 0.9);
    if (env.isDebugMatch())
        LOG(INFO) << "LineDetector found "<<linesFound<<" lines, x0=" << x0 << " x1="<<x1;
    std::array<cv::Point2f,2> points { cv::Point2f(x0,y0-2), cv::Point2f(x1,y0-2) };
    cv::Matx23d invAffineMatrix;
    cv::invertAffineTransform(affineMatrix, invAffineMatrix);
    cv::transform(points, points, invAffineMatrix);
    captureP0 = cv::Point(points[0]) + lineMatchRect.tl();
    captureP1 = cv::Point(points[1]) + lineMatchRect.tl();
    detectedLine = {captureP0, captureP1};
    lastDeltaScale = cv::norm(captureP1 - captureP0) / captureWidth;

    ClassifiedRect& cr = env.classified.emplace_back(ClsDetType::LineDetected, env.isWarpMode(),
                                name + ':' + an->imagesPrepared[an->lastTemplatedx].name,
                                anRefRect + env.scaleToReference(an->matchedCaptureOffset));
    cr.u.ldet.referenceLine = env.cvtCapturedToReference(detectedLine);
    cr.u.ldet.scale = lastDeltaScale;
    cr.u.ldet.angle = lastDeltaAngle;
    cr.u.ldet.match = an->lastMatch;
    cr.u.ldet.offset = env.scaleToReference(an->matchedCaptureOffset);
    cr.u.ldet.detector = this;
    return 1;
}

SimpleLineDetector::SimpleLineDetector(spEvalLine line)
        : LineDetector(line)
{
}


double SimpleLineDetector::match(ClassifyEnv &env) {
    cv::Line refLine;
    if (altReferenceLine)
        refLine = altReferenceLine->calcReferenceLine(env);
    else
        refLine = referenceLine->calcReferenceLine(env);
    expectedLine = env.cvtReferenceToCaptured(refLine);
    cv::Point captureP0 = expectedLine.p0();
    cv::Point captureP1 = expectedLine.p1();
    const int captureWidth = cv::norm(captureP1 - captureP0);
    cv::Rect r0 = cv::Rect(captureP0 - env.scaleToCaptured(extendLT),
                           captureP0 + env.scaleToCaptured(extendRB));
    cv::Rect r1 = cv::Rect(captureP1 - env.scaleToCaptured(extendLT),
                           captureP1 + env.scaleToCaptured(extendRB));
    lineMatchRect = r0 | r1;
    env.cropToCapture(lineMatchRect);

    XMat imagePrepared = ImageTemplate::applyFilters(filters, env.getColorImage()(lineMatchRect));
    //cv::threshold(imagePrepared, imagePrepared, 127, 255, cv::THRESH_BINARY);

    cv::Point2f captureAnchor = (captureP0 + captureP1) * 0.5;
    cv::Point referenceDist = refLine.p1() - refLine.p0();
    double referenceAngle = std::atan2(referenceDist.y, referenceDist.x) * 180 / CV_PI;
    double avrgAngle = 0; // degrees
    double avrgAnchorDist = 0; // pixels
    int linesFound = 0;

    cv::Point2f matchAnchor = captureAnchor - cv::Point2f(lineMatchRect.tl());
    int threshold;
    if (houghThreshold > 0)
        threshold = houghThreshold * env.getScale() * env.getScale();
    else
        threshold = captureWidth * 0.95;
    double expAngle = 90+referenceAngle;
    {
        cv::Mat linesX;
        cv::HoughLines(imagePrepared, linesX, 1, angleStep * CV_PI / 180, threshold, 0, 0,
                       (expAngle + extendAngleMin) * CV_PI / 180, (expAngle + extendAngleMax) * CV_PI / 180);
        //LOG(DEBUG) << "LineDetector '" << name << "' found " << linesX.rows << " lines";
        if (linesX.rows <= 0)
            return 0;
        cv::Mat lines = linesX; //toMat(linesX);
        for (int ln = 0; ln < lines.rows; ln++) {
            auto &lv = lines.at<cv::Vec2f>(ln);
            float rho = lv[0];
            float angle = lv[1]; // radians
            double cos_a = cos(angle);
            double sin_a = sin(angle);
            double x0 = cos_a * rho;
            double y0 = sin_a * rho;
            cv::Point pt1{cvRound(x0 + 100 * (-sin_a)), cvRound(y0 + 100 * (cos_a))};
            cv::Point pt2{cvRound(x0 - 100 * (-sin_a)), cvRound(y0 - 100 * (cos_a))};

            double anchorDist = distanceToLine(matchAnchor, cv::Line2d(pt1, pt2));
            avrgAngle += angle * 180 / CV_PI - 90;
            avrgAnchorDist += anchorDist;
            linesFound += 1;
        }
    }

    if (!linesFound)
        return 0;

    avrgAngle /= linesFound;
    avrgAnchorDist /= linesFound;
    lastLineAngle = avrgAngle;
    lastDeltaAngle = avrgAngle - referenceAngle;

    cv::Matx23d affineMatrix = cv::getRotationMatrix2D_(matchAnchor, avrgAngle, 1);
    affineMatrix.val[5] -= avrgAnchorDist + matchAnchor.y - 2;
    cv::Size lineImageSize {imagePrepared.cols, 5};
    cv::Mat lineImage;
    cv::warpAffine(imagePrepared, lineImage, affineMatrix, lineImageSize, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);

    int y0, x0, x1;
    extractLine(lineImage, y0, x0, x1, captureWidth * 0.8);
    if (env.isDebugMatch())
        LOG(INFO) << "LineDetector found "<<linesFound<<" lines, x0=" << x0 << " x1="<<x1;
    std::array<cv::Point2f,2> points { cv::Point2f(x0,y0-2), cv::Point2f(x1,y0-2) };
    cv::Matx23d invAffineMatrix;
    cv::invertAffineTransform(affineMatrix, invAffineMatrix);
    cv::transform(points, points, invAffineMatrix);
    captureP0 = cv::Point(points[0]) + lineMatchRect.tl();
    captureP1 = cv::Point(points[1]) + lineMatchRect.tl();
    detectedLine = {captureP0, captureP1};
    lastDeltaScale = cv::norm(captureP1 - captureP0) / captureWidth;

    ClassifiedRect& cr = env.classified.emplace_back(ClsDetType::LineDetected, env.isWarpMode(), name + ':', cv::Rect());
    cr.u.ldet.referenceLine = env.cvtCapturedToReference(detectedLine);
    cr.u.ldet.scale = lastDeltaScale;
    cr.u.ldet.angle = lastDeltaAngle;
    cr.u.ldet.match = 1;
    cr.u.ldet.offset = expectedLine.p0()-detectedLine.p0();
    cr.u.ldet.detector = this;
    return 1;
}

} // detect
