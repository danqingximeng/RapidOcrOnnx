#ifndef __JNI__
#ifndef __CLIB__
#include <cstdio>
#include "main.h"
#include "version.h"
#include "OcrLite.h"
#include "OcrUtils.h"
#include "nlohmann/json.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

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
    int maxSideLen = 1024;
    int minSideLen = 640;
    int maxSideLimit = 8192;
    float boxScoreThresh = 0.5f;
    float boxThresh = 0.3f;
    float unClipRatio = 1.5f;
    bool doAngle = true;
    int flagDoAngle = 1;
    bool mostAngle = true;
    int flagMostAngle = 1;
    int flagGpu = -1;
    bool debug = false;
    bool saveImg = false;
    std::string saveImgPath;

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
                maxSideLen = (int) strtol(optarg, NULL, 10);
                break;
            case OPT_MIN_SIDE:
                minSideLen = (int) strtol(optarg, NULL, 10);
                break;
            case OPT_MAX_SIDE_LIMIT:
                maxSideLimit = (int) strtol(optarg, NULL, 10);
                break;
            case 'b':
                boxScoreThresh = strtof(optarg, NULL);
                break;
            case 'o':
                boxThresh = strtof(optarg, NULL);
                break;
            case 'u':
                unClipRatio = strtof(optarg, NULL);
                break;
            case 'a':
                flagDoAngle = (int) strtol(optarg, NULL, 10);
                doAngle = (flagDoAngle != 0);
                break;
            case 'A':
                flagMostAngle = (int) strtol(optarg, NULL, 10);
                mostAngle = (flagMostAngle != 0);
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
            default:
                printErrorJson(202, "invalid option");
                return 1;
        }
    }

    if (imgPath.empty()) {
        printErrorJson(202, "missing required parameter: -i/--image");
        return 1;
    }
    if (!isFileExists(imgPath)) {
        printErrorJson(200, "image not found: " + imgPath);
        return 1;
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
            "numThread(%d),maxSideLen(%d),minSideLen(%d),maxSideLimit(%d),boxScoreThresh(%f),boxThresh(%f),unClipRatio(%f),doAngle(%d),mostAngle(%d),GPU(%d)\n",
            numThread, maxSideLen, minSideLen, maxSideLimit, boxScoreThresh, boxThresh, unClipRatio, doAngle, mostAngle,
            flagGpu);

    OcrResult result;
    try {
        if (!ocrLite.initModels(modelDetPath, modelClsPath, modelRecPath, keysPath)) {
            printErrorJson(201, "init models failed");
            return 1;
        }
        result = ocrLite.detect(imgDir.c_str(), imgName.c_str(), maxSideLen, minSideLen, maxSideLimit,
                                boxScoreThresh, boxThresh, unClipRatio, doAngle, mostAngle);
    } catch (const std::exception &e) {
        printErrorJson(201, e.what());
        return 1;
    }

    json data = json::array();
    double clsTotal = 0.0;
    double recTotal = 0.0;
    for (const auto &block: result.textBlocks) {
        json item;
        item["text"] = block.text;
        json box = json::array();
        for (const auto &p: block.boxPoint) {
            box.push_back(json::array({p.x, p.y}));
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
            {"image",   imgPath},
            {"models",  {{"det", modelDetPath},
                         {"cls", modelClsPath},
                         {"rec", modelRecPath},
                         {"keys", keysPath}}},
            {"params",  {{"numThread",      numThread},
                         {"maxSideLen",     maxSideLen},
                         {"minSideLen",     minSideLen},
                         {"maxSideLimit",   maxSideLimit},
                         {"boxScoreThresh", boxScoreThresh},
                         {"boxThresh",      boxThresh},
                         {"unClipRatio",    unClipRatio},
                         {"doAngle",        flagDoAngle},
                         {"mostAngle",      flagMostAngle},
                         {"gpu",            flagGpu}}}
    };
    printf("%s\n", out.dump().c_str());
    fflush(stdout);
    return 0;
}

#endif
#endif
