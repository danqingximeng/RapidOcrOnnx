#ifndef __OCR_ANGLENET_H__
#define __OCR_ANGLENET_H__

#include "OcrStruct.h"
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

class AngleNet {
public:

    ~AngleNet();

    void setNumThread(int numOfThread);

    void setGpuIndex(int gpuIndex);

    void initModel(const std::string &pathStr);

    int getDstWidth() const { return dstWidth; }

    int getDstHeight() const { return dstHeight; }

    bool isFixedInputShape() const { return fixedInputShape; }

    std::vector<Angle> getAngles(std::vector<cv::Mat> &partImgs, const char *path,
                                 const char *imgName, bool doAngle, bool mostAngle);

private:
    bool isOutputAngleImg = false;

    Ort::Session *session = nullptr;
    Ort::Env env = Ort::Env(ORT_LOGGING_LEVEL_ERROR, "AngleNet");
    Ort::SessionOptions sessionOptions = Ort::SessionOptions();
    int numThread = 0;

    std::vector<Ort::AllocatedStringPtr> inputNamesPtr;
    std::vector<Ort::AllocatedStringPtr> outputNamesPtr;

    const float meanValues[3] = {127.5, 127.5, 127.5};
    const float normValues[3] = {1.0 / 127.5, 1.0 / 127.5, 1.0 / 127.5};
    // Auto-detected from ONNX input shape at initModel: old MobileNetV3 cls 192x48, new PP-LCNet textline_ori 160x80
    int dstWidth = 192;
    int dstHeight = 48;
    bool fixedInputShape = false;
    //PaddleOCR default cls_thresh (tools/infer/predict_cls.py)
    const float clsScoreThresh = 0.9f;

    Angle getAngle(cv::Mat &src);

    void detectInputShape();
};


#endif //__OCR_ANGLENET_H__
