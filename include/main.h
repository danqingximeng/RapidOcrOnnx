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
    OPT_DIR = 1007
};

static const struct option long_options[] = {
        {"dir",            required_argument, NULL, OPT_DIR},
        {"det",            required_argument, NULL, OPT_DET},
        {"cls",            required_argument, NULL, OPT_CLS},
        {"rec",            required_argument, NULL, OPT_REC},
        {"key",            required_argument, NULL, OPT_KEY},
        {"image",          required_argument, NULL, 'i'},
        {"thread",         required_argument, NULL, 't'},
        {"padding",        required_argument, NULL, 'p'},
        {"max-side",       required_argument, NULL, 's'},
        {"box-score",      required_argument, NULL, 'b'},
        {"thresh",         required_argument, NULL, 'o'},
        {"unclip-ratio",   required_argument, NULL, 'u'},
        {"do-angle",       required_argument, NULL, 'a'},
        {"most-angle",     required_argument, NULL, 'A'},
        {"version",        no_argument,       NULL, 'v'},
        {"help",           no_argument,       NULL, 'h'},
        {"loop",           required_argument, NULL, 'l'},
        {"gpu",            required_argument, NULL, 'G'},
        {"debug",          no_argument,       NULL, OPT_DEBUG},
        {"save-img",       optional_argument, NULL, OPT_SAVE_IMG},
        {NULL,             no_argument,       NULL, 0}
};

const char *usageMsg = "(-i --image) (--det) (--cls) (--rec) (--key) [--dir]\n"\
                       "[--thread] [--padding] [--max-side] [--box-score] [--thresh] [--unclip-ratio]\n" \
                       "[--do-angle] [--most-angle] [--gpu]\n" \
                       "[--save-img] [--debug]\n" \
                       "Result: JSON printed to stdout only.\n\n";

const char *requiredMsg = "-i --image: path of target image.\n" \
                          "--det: path of det model file.\n" \
                          "--cls: path of cls model file.\n" \
                          "--rec: path of rec model file.\n" \
                          "--key: path of keys file.\n\n";

const char *optionalMsg = "--dir: directory prefix; when set, --det/--cls/--rec/--key take file names inside it\n" \
                          "--thread: value of numThread(int), default: 4\n" \
                          "--padding: value of padding(int), default: 50\n" \
                          "--max-side: Long side of picture for resize(int), default: 1024\n" \
                          "--box-score: box confidence threshold(float), default: 0.5\n" \
                          "--thresh: prob map binarization threshold(float), default: 0.3\n" \
                          "--unclip-ratio: box expansion ratio(float), default: 1.6\n" \
                          "--do-angle: Enable(1)/Disable(0) Angle Net, default: Enable\n" \
                          "--most-angle: Enable(1)/Disable(0) Most Possible AngleIndex, default: Enable\n" \
                          "--gpu: Disable(-1)/GPU0(0)/GPU1(1)/... Use Vulkan GPU accelerate, default: Disable(-1)\n" \
                          "--save-img: write the box-drawn image, default <image>-result.jpg," \
                          " or use --save-img=PATH to choose the path\n" \
                          "--debug: detailed logs to stderr (stdout stays a single JSON), default: off\n\n";

const char *otherMsg = "-v --version: show version\n" \
                       "-h --help: print this help\n" \
                       "Exit codes: 0 success (JSON code 100/101), 1 error (JSON code 200/201/202)\n\n";

const char *example1Msg = "Example1: %s --det /models/small_det.onnx --cls /models/cls.onnx --rec /models/small_rec.onnx --key /models/keys.txt --image 1.jpg\n";
const char *example2Msg = "Example2: %s --dir models --det small_det.onnx --cls cls.onnx --rec small_rec.onnx --key keys.txt -i 1.jpg --thread 4 --padding 50 --max-side 1024 --box-score 0.5 --thresh 0.3 --unclip-ratio 1.6 --do-angle 1 --most-angle 1 --gpu -1\n";

#endif //__MAIN_H__
