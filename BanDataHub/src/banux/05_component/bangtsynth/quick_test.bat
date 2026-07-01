@echo off
REM BanGTsynth 快速测试脚本 (Windows版本)
REM 功能: 下载音源 → 编译项目 → 准备运行

echo ========================================
echo BanGTsynth 快速测试
echo ========================================
echo.

REM 步骤1: 下载音源
echo ^>^>^> 步骤1: 下载音源到 soundbank.bin
echo.

REM 检查SF2文件
set SF2_FILE=
if exist "..\sf2\Full Grand.sf2" (
    set SF2_FILE=..\sf2\Full Grand.sf2
) else if exist "soundbank\piano\piano.sf2" (
    set SF2_FILE=soundbank\piano\piano.sf2
) else (
    echo 错误: 找不到SF2文件
    echo 请将SF2文件放置在以下位置之一:
    echo   - ..\sf2\Full Grand.sf2
    echo   - soundbank\piano\piano.sf2
    pause
    exit /b 1
)

echo 使用音源: %SF2_FILE%
echo.

REM 使用PowerShell下载
powershell -ExecutionPolicy Bypass -File download_soundbank.ps1 "%SF2_FILE%" 0

if errorlevel 1 (
    echo.
    echo 下载失败!
    pause
    exit /b 1
)

echo.

REM 步骤2: 编译项目
echo ^>^>^> 步骤2: 编译项目
echo.

if not exist build mkdir build
cd build
cmake ..
cmake --build . --config Release
cd ..

if errorlevel 1 (
    echo.
    echo 编译失败!
    pause
    exit /b 1
)

echo.

REM 步骤3: 验证soundbank.bin
echo ^>^>^> 步骤3: 验证 soundbank.bin
echo.

if exist soundbank.bin (
    for %%A in (soundbank.bin) do set FILE_SIZE=%%~zA
    echo √ soundbank.bin 存在
    echo   文件大小: %FILE_SIZE% bytes
) else (
    echo × soundbank.bin 不存在!
    pause
    exit /b 1
)

echo.

REM 步骤4: 运行测试
echo ^>^>^> 步骤4: 准备运行
echo.
echo ========================================
echo 测试准备完成!
echo ========================================
echo.
echo 运行主程序:
echo   cd build ^&^& demo.exe
echo.
echo 或者手动测试下载接口:
echo   cd example\download_example\build
echo   download_example.exe ..\..\soundbank\piano.sf2 0
echo.
pause
