#ifndef __JNI__
#ifndef __CLIB__
#include <cstdio>
#include <iostream>
#include "main.h"
#include "version.h"
#include "OcrLite.h"
#include "OcrUtils.h"
#include "nlohmann/json.hpp"
#include <opencv2/imgcodecs.hpp>
#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

// Detection parameters shared by single-shot mode and per-request server overrides.
// Field names match the CLI flags / JSON meta.params keys.
struct DetectParams {
    int maxSide = 1024;
    int minSide = 640;
    int maxSideLimit = 8192;
    int upscale = 0;
    float boxThresh = 0.5f;
    float thresh = 0.3f;
    float unclipRatio = 1.5f;
    int doAngle = 1;
    int mostAngle = 1;
};

void printHelp(FILE *out, char *argv0) {
    fprintf(out, " ------- Usage -------\n");
    fprintf(out, "%s %s", argv0, usageMsg);
    fprintf(out, " ------- Required Parameters -------\n");
    fprintf(out, "%s", requiredMsg);
    fprintf(out, " ------- Optional Parameters -------\n");
    fprintf(out, "%s", optionalMsg);
    fprintf(out, " ------- Other Parameters -------\n");
    fprintf(out, "%s", otherMsg);
    fprintf(out, " ------- Examples -------\n");
    fprintf(out, example1Msg, argv0);
    fprintf(out, example2Msg, argv0);
}

static void printErrorJson(int code, const std::string &message) {
    json out;
    out["code"] = code;
    out["message"] = message;
    printf("%s\n", out.dump().c_str());
    fflush(stdout);
}

static json buildResultJson(const OcrResult &result, const std::string &image,
                            const std::string &modelDetPath, const std::string &modelClsPath,
                            const std::string &modelRecPath, const std::string &keysPath,
                            int numThread, int flagGpu, const DetectParams &p) {
    json data = json::array();
    double clsTotal = 0.0;
    double recTotal = 0.0;
    for (const auto &block: result.textBlocks) {
        json item;
        item["text"] = block.text;
        json box = json::array();
        for (const auto &pt: block.boxPoint) {
            box.push_back(json::array({pt.x, pt.y}));
        }
        item["box"] = box;
        item["boxScore"] = block.boxScore;
        float scoreSum = 0.0f;
        for (float s: block.charScores) {
            scoreSum += s;
        }
        item["score"] = block.charScores.empty() ? 0.0f
                                                 : scoreSum / (float) block.charScores.size();
        item["scores"] = block.charScores;
        item["angle"] = block.angleIndex;
        item["angleScore"] = block.angleScore;
        item["time"] = {{"cls", block.angleTime}, {"rec", block.crnnTime}};
        clsTotal += block.angleTime;
        recTotal += block.crnnTime;
        data.push_back(item);
    }

    json out;
    out["code"] = result.textBlocks.empty() ? 101 : 100;
    out["data"] = data;
    out["time"] = {{"det",   result.dbNetTime},
                   {"cls",   clsTotal},
                   {"rec",   recTotal},
                   {"total", result.detectTime}};
    out["meta"] = {
            {"version", VERSION},
            {"image",   image},
            {"models",  {{"det", modelDetPath},
                         {"cls", modelClsPath},
                         {"rec", modelRecPath},
                         {"keys", keysPath}}},
            {"scale",   {{"srcWidth",    result.scale.srcWidth},
                         {"srcHeight",   result.scale.srcHeight},
                         {"dstWidth",    result.scale.dstWidth},
                         {"dstHeight",   result.scale.dstHeight},
                         {"ratioWidth",  result.scale.ratioWidth},
                         {"ratioHeight", result.scale.ratioHeight}}},
            {"params",  {{"thread",      numThread},
                         {"maxSide",     p.maxSide},
                         {"minSide",     p.minSide},
                         {"maxSideLimit", p.maxSideLimit},
                         {"upscale",     p.upscale},
                         {"boxThresh",   p.boxThresh},
                         {"thresh",      p.thresh},
                         {"unclipRatio", p.unclipRatio},
                         {"doAngle",     p.doAngle},
                         {"mostAngle",   p.mostAngle},
                         {"gpu",         flagGpu}}}
    };
    return out;
}

static bool base64Decode(const std::string &in, std::vector<uchar> &out) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static int table[256];
    static bool tableReady = false;
    if (!tableReady) {
        std::fill(std::begin(table), std::end(table), -1);
        for (int i = 0; i < (int) chars.size(); ++i) {
            table[(unsigned char) chars[i]] = i;
        }
        tableReady = true;
    }
    int val = 0, valb = -8;
    for (unsigned char c: in) {
        if (c == '=') break;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        if (table[c] == -1) return false;
        val = (val << 6) + table[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back((uchar) ((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return !out.empty();
}

static int runServer(OcrLite &ocrLite, const DetectParams &defaults,
                     const std::string &modelDetPath, const std::string &modelClsPath,
                     const std::string &modelRecPath, const std::string &keysPath,
                     int numThread, int flagGpu) {
    printf("RapidOcrOnnx v%s\n", VERSION);
    printf("OCR init completed.\n");
    fflush(stdout);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        json req;
        try {
            req = json::parse(line);
        } catch (const json::exception &) {
            printErrorJson(202, "invalid json input");
            continue;
        }

        std::string imgPath, imgB64;
        if (req.contains("image_path") && req["image_path"].is_string()) {
            imgPath = req["image_path"].get<std::string>();
        }
        if (req.contains("image_base64") && req["image_base64"].is_string()) {
            imgB64 = req["image_base64"].get<std::string>();
        }
        if (imgPath.empty() && imgB64.empty()) {
            printErrorJson(202, "missing image_path or image_base64");
            continue;
        }

        DetectParams p = defaults;
        try {
            if (req.contains("maxSide")) p.maxSide = req["maxSide"].get<int>();
            if (req.contains("minSide")) p.minSide = req["minSide"].get<int>();
            if (req.contains("maxSideLimit")) p.maxSideLimit = req["maxSideLimit"].get<int>();
            if (req.contains("upscale")) p.upscale = req["upscale"].get<int>() != 0 ? 1 : 0;
            if (req.contains("boxThresh")) p.boxThresh = req["boxThresh"].get<float>();
            if (req.contains("thresh")) p.thresh = req["thresh"].get<float>();
            if (req.contains("unclipRatio")) p.unclipRatio = req["unclipRatio"].get<float>();
            if (req.contains("doAngle")) p.doAngle = req["doAngle"].get<int>() != 0 ? 1 : 0;
            if (req.contains("mostAngle")) p.mostAngle = req["mostAngle"].get<int>() != 0 ? 1 : 0;
        } catch (const json::exception &e) {
            printErrorJson(202, std::string("invalid param override: ") + e.what());
            continue;
        }

        cv::Mat img;
        std::string image;
        if (!imgPath.empty()) {
            image = imgPath;
            if (!isFileExists(imgPath)) {
                printErrorJson(200, "image not found: " + imgPath);
                continue;
            }
            img = cv::imread(imgPath, cv::IMREAD_COLOR);
            if (img.empty()) {
                printErrorJson(201, "image load failed: " + imgPath);
                continue;
            }
        } else {
            image = "<base64>";
            std::vector<uchar> data;
            if (!base64Decode(imgB64, data)) {
                printErrorJson(201, "base64 decode failed");
                continue;
            }
            img = cv::imdecode(data, cv::IMREAD_COLOR);
            if (img.empty()) {
                printErrorJson(201, "base64 image decode failed");
                continue;
            }
        }

        try {
            OcrResult result = ocrLite.detect(img, p.maxSide, p.minSide, p.maxSideLimit, p.upscale != 0,
                                              p.boxThresh, p.thresh, p.unclipRatio,
                                              p.doAngle != 0, p.mostAngle != 0);
            json out = buildResultJson(result, image, modelDetPath, modelClsPath, modelRecPath, keysPath,
                                       numThread, flagGpu, p);
            printf("%s\n", out.dump().c_str());
            fflush(stdout);
        } catch (const std::exception &e) {
            printErrorJson(201, e.what());
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        printHelp(stderr, argv[0]);
        return 1;
    }
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::string modelsDir, modelDetPath, modelClsPath, modelRecPath, keysPath;
    std::string imgPath, imgDir, imgName;
    int numThread = 4;
    int flagGpu = -1;
    bool debug = false;
    bool saveImg = false;
    std::string saveImgPath;
    bool server = false;
    DetectParams params;

    int opt;
    int optionIndex = 0;
    auto modelPath = [&modelsDir](const char *arg) {
        return modelsDir.empty() ? std::string(arg) : modelsDir + "/" + arg;
    };
    while ((opt = getopt_long(argc, argv, "i:hv", long_options, &optionIndex)) != -1) {
        switch (opt) {
            case OPT_DIR:
                modelsDir = optarg;
                break;
            case OPT_DET:
                modelDetPath = modelPath(optarg);
                break;
            case OPT_CLS:
                modelClsPath = modelPath(optarg);
                break;
            case OPT_REC:
                modelRecPath = modelPath(optarg);
                break;
            case OPT_KEY:
                keysPath = modelPath(optarg);
                break;
            case 'i':
                imgPath.assign(optarg);
                imgDir.assign(imgPath.substr(0, imgPath.find_last_of('/') + 1));
                imgName.assign(imgPath.substr(imgPath.find_last_of('/') + 1));
                break;
            case 't':
                numThread = (int) strtol(optarg, NULL, 10);
                break;
            case 's':
                params.maxSide = (int) strtol(optarg, NULL, 10);
                break;
            case OPT_MIN_SIDE:
                params.minSide = (int) strtol(optarg, NULL, 10);
                break;
            case OPT_MAX_SIDE_LIMIT:
                params.maxSideLimit = (int) strtol(optarg, NULL, 10);
                break;
            case OPT_UPSCALE:
                params.upscale = (int) strtol(optarg, NULL, 10) != 0 ? 1 : 0;
                break;
            case 'b':
                params.boxThresh = strtof(optarg, NULL);
                break;
            case 'o':
                params.thresh = strtof(optarg, NULL);
                break;
            case 'u':
                params.unclipRatio = strtof(optarg, NULL);
                break;
            case 'a':
                params.doAngle = (int) strtol(optarg, NULL, 10) != 0 ? 1 : 0;
                break;
            case 'A':
                params.mostAngle = (int) strtol(optarg, NULL, 10) != 0 ? 1 : 0;
                break;
            case 'v':
                printf("%s\n", VERSION);
                return 0;
            case 'h':
                printHelp(stdout, argv[0]);
                return 0;
            case 'G':
                flagGpu = (int) strtol(optarg, NULL, 10);
                break;
            case OPT_DEBUG:
                debug = true;
                break;
            case OPT_SAVE_IMG:
                saveImg = true;
                if (optarg != NULL) {
                    saveImgPath = optarg;
                }
                break;
            case OPT_SERVER:
                server = true;
                break;
            default:
                printErrorJson(202, "invalid option");
                return 1;
        }
    }

    if (server && !imgPath.empty()) {
        printErrorJson(202, "--server cannot be combined with -i/--image");
        return 1;
    }
    if (server && saveImg && saveImgPath.empty()) {
        // Mat-based detect has no source path to derive a result image name from.
        printErrorJson(202, "--save-img in --server mode requires an explicit path");
        return 1;
    }
    if (!server) {
        if (imgPath.empty()) {
            printErrorJson(202, "missing required parameter: -i/--image");
            return 1;
        }
        if (!isFileExists(imgPath)) {
            printErrorJson(200, "image not found: " + imgPath);
            return 1;
        }
    }
    if (modelDetPath.empty() || !isFileExists(modelDetPath)) {
        printErrorJson(202, "det model file not found: " + modelDetPath);
        return 1;
    }
    if (modelClsPath.empty() || !isFileExists(modelClsPath)) {
        printErrorJson(202, "cls model file not found: " + modelClsPath);
        return 1;
    }
    if (modelRecPath.empty() || !isFileExists(modelRecPath)) {
        printErrorJson(202, "rec model file not found: " + modelRecPath);
        return 1;
    }
    if (keysPath.empty() || !isFileExists(keysPath)) {
        printErrorJson(202, "keys file not found: " + keysPath);
        return 1;
    }

    OcrLite ocrLite;
    ocrLite.setNumThread(numThread);
    ocrLite.initLogger(
            debug,//isOutputConsole, logs to stderr so stdout stays pure JSON
            false,//isOutputPartImg
            saveImg);//isOutputResultImg
    if (!saveImgPath.empty()) {
        ocrLite.setResultImgPath(saveImgPath);
    }
    ocrLite.setGpuIndex(flagGpu);
    ocrLite.Logger("=====Input Params=====\n");
    ocrLite.Logger(
            "thread(%d),maxSide(%d),minSide(%d),maxSideLimit(%d),upscale(%d),boxThresh(%f),thresh(%f),unclipRatio(%f),doAngle(%d),mostAngle(%d),gpu(%d)\n",
            numThread, params.maxSide, params.minSide, params.maxSideLimit, params.upscale, params.boxThresh,
            params.thresh, params.unclipRatio, params.doAngle, params.mostAngle, flagGpu);

    try {
        if (!ocrLite.initModels(modelDetPath, modelClsPath, modelRecPath, keysPath)) {
            printErrorJson(201, "init models failed");
            return 1;
        }
    } catch (const std::exception &e) {
        printErrorJson(201, e.what());
        return 1;
    }

    if (server) {
        return runServer(ocrLite, params, modelDetPath, modelClsPath, modelRecPath, keysPath, numThread, flagGpu);
    }

    OcrResult result;
    try {
        result = ocrLite.detect(imgDir.c_str(), imgName.c_str(), params.maxSide, params.minSide, params.maxSideLimit,
                                params.upscale != 0, params.boxThresh, params.thresh, params.unclipRatio,
                                params.doAngle != 0, params.mostAngle != 0);
    } catch (const std::exception &e) {
        printErrorJson(201, e.what());
        return 1;
    }

    json out = buildResultJson(result, imgPath, modelDetPath, modelClsPath, modelRecPath, keysPath,
                               numThread, flagGpu, params);
    printf("%s\n", out.dump().c_str());
    fflush(stdout);
    return 0;
}

#endif
#endif
