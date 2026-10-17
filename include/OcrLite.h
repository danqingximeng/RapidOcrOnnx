#ifndef __OCR_LITE_H__
#define __OCR_LITE_H__

#include <string>
#include "OcrStruct.h"

class OcrLiteImpl;

class OcrLite{
public:
    OcrLite();

    ~OcrLite();

    void setNumThread(int numOfThread);

    void initLogger(bool isConsole, bool isPartImg, bool isResultImg);

    void setResultImgPath(const std::string &imgPath);

    void setGpuIndex(int gpuIndex);

    // Reading-order sort ("gaptree" | "none") and paragraph end prediction.
    void setSortMode(const std::string &sortMode);

    void setParagraph(bool paragraph);

    bool initModels(const std::string &detPath, const std::string &clsPath,
                    const std::string &recPath, const std::string &keysPath);

    void Logger(const char *format, ...);

    OcrResult detect(const char *path, const char *imgName,
                     int maxSideLen, int minSideLen, int maxSideLimit, bool upscale,
                     float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle);

    OcrResult detect(const cv::Mat &mat,
                     int maxSideLen, int minSideLen, int maxSideLimit, bool upscale,
                     float boxScoreThresh, float boxThresh, float unClipRatio, bool doAngle, bool mostAngle);

private:
    OcrLiteImpl* pImpl;
};

#endif //__OCR_LITE_H__
