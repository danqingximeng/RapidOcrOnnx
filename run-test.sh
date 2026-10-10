#!/usr/bin/env bash

function PrepareVar(){
echo "onnxruntime: 1)CPU(默认), 2)GPU(cuda)"
read -p "" ONNX_TYPE
if [ $ONNX_TYPE == 1 ]; then
  ONNX_TYPE="CPU"
  GPU_INDEX=-1
elif [ $ONNX_TYPE == 2 ]; then
  ONNX_TYPE="CUDA"
  GPU_INDEX=0
else
  echo -e "输入错误！Input Error!"
fi
EXE_PATH=${sysOS}-${ONNX_TYPE}-BIN
}

sysOS=`uname -s`
NUM_THREADS=1
if [ $sysOS == "Darwin" ];then
    #echo "I'm MacOS"
    NUM_THREADS=$(sysctl -n hw.ncpu)
elif [ $sysOS == "Linux" ];then
    #echo "I'm Linux"
    NUM_THREADS=$(grep ^processor /proc/cpuinfo | wc -l)
else
    echo "Other OS: $sysOS"
fi

echo "Setting the Number of Threads=$NUM_THREADS Using an OpenMP Environment Variable"
set OMP_NUM_THREADS=$NUM_THREADS

PrepareVar

TARGET_IMG=images/1.jpg
if [ ! -f "$TARGET_IMG" ]; then
echo "找不到待识别的目标图片：${TARGET_IMG}，请打开本文件并编辑TARGET_IMG"
exit
fi

##### run test on MacOS or Linux
./${EXE_PATH}/RapidOcrOnnx --version
./${EXE_PATH}/RapidOcrOnnx --dir models \
--det PP-OCRv6_small_det.onnx \
--cls ch_ppocr_mobile_v2.0_cls_infer.onnx \
--rec PP-OCRv6_small_rec.onnx \
--key ppocrv6_small_dict.txt \
--image $TARGET_IMG \
--thread $NUM_THREADS \
--max-side 1024 \
--min-side 640 \
--max-side-limit 8192 \
--thresh 0.3 \
--box-thresh 0.5 \
--unclip-ratio 1.5 \
--do-angle 1 \
--most-angle 1 \
--gpu $GPU_INDEX