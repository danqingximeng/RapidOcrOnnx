chcp 65001
:: Set Param
@ECHO OFF
@SETLOCAL
echo "Setting the Number of Threads=%NUMBER_OF_PROCESSORS% Using an OpenMP Environment Variable"
set OMP_NUM_THREADS=%NUMBER_OF_PROCESSORS%

:MainExec
echo "请输入测试选项并回车: 1)CPU-x64, 2)CPU-x86, 3)CUDA-x64"
set /p flag=
if %flag% == 1 (call :PrepareCpuX64)^
else if %flag% == 2 (call :PrepareCpuX86)^
else if %flag% == 3 (call :PrepareCudaX64)^
else (echo 输入错误！Input Error!)

SET TARGET_IMG=images/1.jpg
if not exist %TARGET_IMG% (
echo "找不到待识别的目标图片：%TARGET_IMG%，请打开本文件并编辑TARGET_IMG"
PAUSE
exit
)

if exist %EXE_PATH%\install\bin (
SET EXE_PATH=%EXE_PATH%\install\bin
)

%EXE_PATH%\RapidOcrOnnx.exe --version
%EXE_PATH%\RapidOcrOnnx.exe --dir models ^
--det PP-OCRv6_small_det.onnx ^
--cls ch_ppocr_mobile_v2.0_cls_infer.onnx ^
--rec PP-OCRv6_small_rec.onnx  ^
--key ppocrv6_small_dict.txt ^
--image %TARGET_IMG% ^
--thread %NUMBER_OF_PROCESSORS% ^
--max-side 1024 ^
--min-side 640 ^
--max-side-limit 8192 ^
--box-score 0.5 ^
--thresh 0.3 ^
--unclip-ratio 1.6 ^
--do-angle 1 ^
--most-angle 1 ^
--gpu %GPU_INDEX%

echo.
GOTO:MainExec

:PrepareCpuX64
set EXE_PATH=win-BIN-CPU-x64
set GPU_INDEX=-1
GOTO:EOF

:PrepareCpuX86
set EXE_PATH=win-BIN-CPU-Win32
set GPU_INDEX=-1
GOTO:EOF

:PrepareCudaX64
set EXE_PATH=win-BIN-CUDA-x64
set GPU_INDEX=0
GOTO:EOF

@ENDLOCAL
