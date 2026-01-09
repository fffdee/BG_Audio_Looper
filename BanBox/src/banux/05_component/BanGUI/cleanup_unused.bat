@echo off
REM BanGUI 清理脚本 - 删除未使用的文件
REM 请在执行前确认这些文件确实不需要

echo ================================================
echo BanGUI 清理脚本
echo ================================================
echo.

set BANGUI_PATH=src\banux\05_component\BanGUI

echo 以下文件将被删除（或移动到 _deprecated 文件夹）:
echo.
echo 1. ui_system\audio_spectrum_simple.c
echo 2. ui_system\audio_spectrum_simple.h  
echo 3. ui_system\vacal_setting.c
echo 4. ui_system\ui_view_home.c (已迁移到 ui\views\)
echo 5. ui_system\ui_view_home.h
echo 6. ui_system\ui_view_menu.c (已迁移到 ui\views\)
echo 7. ui_system\ui_view_menu.h
echo 8. ui_system\ui_core.c (已迁移到 ui\core\bg_ui.c)
echo 9. ui_system\ui_core.h
echo.

set /p confirm="确认删除这些文件? (y/n): "

if /i "%confirm%"=="y" (
    echo.
    echo 创建 _deprecated 文件夹...
    mkdir %BANGUI_PATH%\_deprecated 2>nul
    
    echo 移动文件到 _deprecated 文件夹...
    
    REM 移动未使用的文件
    move %BANGUI_PATH%\ui_system\audio_spectrum_simple.c %BANGUI_PATH%\_deprecated\ 2>nul
    move %BANGUI_PATH%\ui_system\audio_spectrum_simple.h %BANGUI_PATH%\_deprecated\ 2>nul
    move %BANGUI_PATH%\ui_system\vacal_setting.c %BANGUI_PATH%\_deprecated\ 2>nul
    
    REM 移动旧版视图文件（已迁移）
    move %BANGUI_PATH%\ui_system\ui_view_home.c %BANGUI_PATH%\_deprecated\ 2>nul
    move %BANGUI_PATH%\ui_system\ui_view_home.h %BANGUI_PATH%\_deprecated\ 2>nul
    move %BANGUI_PATH%\ui_system\ui_view_menu.c %BANGUI_PATH%\_deprecated\ 2>nul
    move %BANGUI_PATH%\ui_system\ui_view_menu.h %BANGUI_PATH%\_deprecated\ 2>nul
    
    REM 移动旧版核心文件（已迁移到 bg_ui）
    move %BANGUI_PATH%\ui_system\ui_core.c %BANGUI_PATH%\_deprecated\ 2>nul
    move %BANGUI_PATH%\ui_system\ui_core.h %BANGUI_PATH%\_deprecated\ 2>nul
    
    echo.
    echo 完成! 文件已移动到 _deprecated 文件夹
    echo 如果确认不需要，可以手动删除该文件夹
) else (
    echo.
    echo 操作已取消
)

echo.
pause
