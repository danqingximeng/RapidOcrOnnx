#ifndef __MAIN_H__
#define __MAIN_H__

#include "getopt.h"

enum {
    OPT_DEBUG = 1001,
    OPT_SAVE_IMG = 1002,
    OPT_DET = 1003,
    OPT_CLS = 1004,
    OPT_REC = 1005,
    OPT_KEY = 1006,
    OPT_DIR = 1007,
    OPT_MIN_SIDE = 1008,
    OPT_MAX_SIDE_LIMIT = 1009,
    OPT_UPSCALE = 1010,
    OPT_SERVER = 1011,
    OPT_SORT = 1012,
    OPT_PARA = 1013
};

static const struct option long_options[] = {
        {"dir",            required_argument, NULL, OPT_DIR},
        {"det",            required_argument, NULL, OPT_DET},
        {"cls",            required_argument, NULL, OPT_CLS},
        {"rec",            required_argument, NULL, OPT_REC},
        {"key",            required_argument, NULL, OPT_KEY},
        {"image",          required_argument, NULL, 'i'},
        {"thread",         required_argument, NULL, 't'},
        {"max-side",       required_argument, NULL, 's'},
        {"min-side",       required_argument, NULL, OPT_MIN_SIDE},
        {"max-side-limit", required_argument, NULL, OPT_MAX_SIDE_LIMIT},
        {"upscale",        required_argument, NULL, OPT_UPSCALE},
        {"thresh",         required_argument, NULL, 'o'},
        {"box-thresh",     required_argument, NULL, 'b'},
        {"unclip-ratio",   required_argument, NULL, 'u'},
        {"do-angle",       required_argument, NULL, 'a'},
        {"most-angle",     required_argument, NULL, 'A'},
        {"sort",           required_argument, NULL, OPT_SORT},
        {"para",           required_argument, NULL, OPT_PARA},
        {"version",        no_argument,       NULL, 'v'},
        {"help",           no_argument,       NULL, 'h'},
        {"gpu",            required_argument, NULL, 'G'},
        {"debug",          no_argument,       NULL, OPT_DEBUG},
        {"save-img",       optional_argument, NULL, OPT_SAVE_IMG},
        {"server",         no_argument,       NULL, OPT_SERVER},
        {NULL,             no_argument,       NULL, 0}
};

const char *usageMsg = "(-i --image) (--det) (--cls) (--rec) (--key) [--dir]\n"\
                       "[--thread] [--max-side] [--min-side] [--max-side-limit] [--upscale] [--thresh] [--box-thresh] [--unclip-ratio]\n" \
                       "[--do-angle] [--most-angle] [--gpu] [--sort] [--para]\n" \
                       "[--save-img] [--debug]\n" \
                       "or persistent server mode:\n" \
                       "--server (--det) (--cls) (--rec) (--key) [--dir] [same optional params]\n" \
                       "Result: JSON printed to stdout only.\n\n";

const char *requiredMsg = "-i --image: path of target image.\n" \
                          "--det: path of det model file.\n" \
                          "--cls: path of cls model file.\n" \
                          "--rec: path of rec model file.\n" \
                          "--key: path of keys file.\n\n";

const char *optionalMsg = "--dir: directory prefix; when set, --det/--cls/--rec/--key take file names inside it\n" \
                           "--thread: number of threads(int), default: 4\n" \
                          "--max-side: long-side budget for downscaling(int), 0 disables, applied only when exceeded, default: 1024\n" \
                          "--min-side: short-side floor while downscaling(int), 0 disables, default: 640\n" \
                          "--max-side-limit: hard cap of the long side after resize(int), 0 disables, default: 8192\n" \
                          "--upscale: Enable(1)/Disable(0) upscaling the short side to --min-side, default: Disable\n" \
                          "--thresh: prob map binarization threshold(float), default: 0.3\n" \
                          "--box-thresh: box confidence threshold(float), default: 0.5\n" \
                          "--unclip-ratio: box expansion ratio(float), default: 1.5\n" \
                           "--do-angle: Enable(1)/Disable(0) Angle Net, default: Enable\n" \
                           "--most-angle: Enable(1)/Disable(0) Most Possible AngleIndex, default: Enable\n" \
                           "--sort: reading-order sort of result blocks: gaptree|none, default: gaptree\n" \
                           "--para: Enable(1)/Disable(0) paragraph end prediction (JSON 'end' field), default: Enable\n" \
                           "--gpu: Disable(-1)/GPU0(0)/GPU1(1)/... Use Vulkan GPU accelerate, default: Disable(-1)\n" \
                           "--save-img: write the box-drawn image, default <image>-result.jpg," \
                           " or use --save-img=PATH to choose the path\n" \
                           "--debug: detailed logs to stderr (stdout stays a single JSON), default: off\n" \
                           "--server: persistent mode; incompatible with -i. Prints handshake, then reads one JSON\n" \
                           "  request per stdin line: {\"image_path\":\"...\"} or {\"image_base64\":\"...\"},\n" \
                           "  optionally overriding detection params (maxSide, minSide, maxSideLimit, upscale,\n" \
                           "  thresh, boxThresh, unclipRatio, doAngle, mostAngle, sort, para), writes one result JSON per\n" \
                           "  line to stdout, exits on EOF\n\n";

const char *otherMsg = "-v --version: show version\n" \
                       "-h --help: print this help\n" \
                       "Exit codes: 0 success (JSON code 100/101 or server EOF), 1 error (JSON code 200/201/202)\n\n";

const char *example1Msg = "Example1: %s --det /models/small_det.onnx --cls /models/cls.onnx --rec /models/small_rec.onnx --key /models/keys.txt --image 1.jpg\n";
const char *example2Msg = "Example2: %s --dir models --det small_det.onnx --cls cls.onnx --rec small_rec.onnx --key keys.txt -i 1.jpg --thread 4 --max-side 1024 --min-side 640 --max-side-limit 8192 --upscale 0 --thresh 0.3 --box-thresh 0.5 --unclip-ratio 1.5 --do-angle 1 --most-angle 1 --gpu -1\n";

#endif //__MAIN_H__
