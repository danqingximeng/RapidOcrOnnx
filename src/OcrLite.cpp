#include "OcrLite.h"
#include "OcrLiteImpl.h"
#include <cstdarg>

OcrLite::OcrLite() {
    pImpl = new OcrLiteImpl();
}

OcrLite::~OcrLite() {
    delete pImpl;
}

void OcrLite::setNumThread(int numOfThread) {
    pImpl->setNumThread(numOfThread);
}

void OcrLite::initLogger(bool isConsole, bool isPartImg, bool isResultImg) {
    pImpl->initLogger(isConsole, isPartImg, isResultImg);
}

void OcrLite::setResultImgPath(const std::string &imgPath) {
    pImpl->setResultImgPath(imgPath);
}

void OcrLite::setGpuIndex(int gpuIndex) {
    pImpl->setGpuIndex(gpuIndex);
}

void OcrLite::setSortMode(const std::string &sortMode) {
    pImpl->setSortMode(sortMode);
}

void OcrLite::setParagraph(bool paragraph) {
    pImpl->setParagraph(paragraph);
}

bool OcrLite::initModels(const std::string &detPath, const std::string &clsPath,
                         const std::string &recPath, const std::string &keysPath) {
    return pImpl->initModels(detPath, clsPath, recPath, keysPath);
}

void OcrLite::Logger(const char *format, ...) {
    if (!pImpl->isOutputConsole) return;
    char *buffer = (char *) malloc(8192);
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    fprintf(stderr, "%s", buffer);
    free(buffer);
}

OcrResult OcrLite::detect(const char *path, const char *imgName,
                          int maxSideLen, int minSideLen, int maxSideLimit, bool upscale,
                          float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle) {
    return pImpl->detect(path, imgName, maxSideLen, minSideLen, maxSideLimit, upscale, boxScoreThresh, boxThresh, unClipRatio, doAngle, mostAngle);
}

OcrResult OcrLite::detect(const cv::Mat &mat,
                          int maxSideLen, int minSideLen, int maxSideLimit, bool upscale,
                          float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle) {
    return pImpl->detect(mat, maxSideLen, minSideLen, maxSideLimit, upscale, boxScoreThresh, boxThresh, unClipRatio, doAngle, mostAngle);
}
