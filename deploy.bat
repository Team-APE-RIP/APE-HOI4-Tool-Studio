@echo off
setlocal EnableDelayedExpansion

echo [INIT] Starting deployment script...

:: 1. 强制关闭正在运行的程序
echo [INFO] Killing existing processes...
taskkill /F /IM APEHOI4ToolStudio.exe >nul 2>&1
:: 等待进程释放文件
timeout /t 2 /nobreak >nul

:: ================= CONFIGURATION =================
:: 请检查这些路径是否真实存在于你的电脑上
set QT_BASE_DIR=D:\Qt
set CMAKE_EXE="D:\Program Files\CMake\bin\cmake.exe"

:: 尝试查找 Qt 版本目录
echo [INFO] Searching for Qt 6.x in %QT_BASE_DIR%...
set QT_VERSION_DIR=
for /d %%D in ("%QT_BASE_DIR%\6.*") do (
    set QT_VERSION_DIR=%%D
    echo [INFO] Found Qt version: %%D
    goto :FoundQt
)

:FoundQt
if "%QT_VERSION_DIR%"=="" (
    echo [ERROR] Could not find any Qt 6.x directory in %QT_BASE_DIR%
    echo Please edit this script and set QT_BASE_DIR correctly.
    exit /b 1
)

:: 设置 MinGW 路径
set QT_DIR=%QT_VERSION_DIR%\mingw_64
set MINGW_DIR=%QT_BASE_DIR%\Tools\mingw1310_64

echo [INFO] QT_DIR: %QT_DIR%
echo [INFO] MINGW_DIR: %MINGW_DIR%

:: 检查目录是否存在
if not exist "%QT_DIR%" (
    echo [ERROR] Qt directory not found: %QT_DIR%
    exit /b 1
)
if not exist "%MINGW_DIR%" (
    echo [ERROR] MinGW directory not found: %MINGW_DIR%
    echo Please check if your MinGW version matches 'mingw1120_64' in the script.
    exit /b 1
)

:: =================================================

:: 添加 Qt 和 MinGW 到 PATH
set PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%

:: 检查 cmake
if not exist %CMAKE_EXE% (
    echo [ERROR] CMake not found at: %CMAKE_EXE%
    echo Please install CMake or update the path in this script.
    exit /b 1
)

echo [INFO] Building project...

:: 2. 清理旧的构建文件和部署文件
if exist build (
    echo [INFO] Cleaning old build directory...
    rmdir /s /q build
    if exist build (
        echo [ERROR] Failed to clean build directory. Is the app still running?
        exit /b 1
    )
)
if exist bin (
    echo [INFO] Cleaning old bin directory...
    rmdir /s /q bin
    if exist bin (
        echo [ERROR] Failed to clean bin directory. Is the app still running?
        exit /b 1
    )
)

mkdir build
cd build

echo [INFO] Running CMake configuration...
%CMAKE_EXE% -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="%QT_DIR%" -DCMAKE_BUILD_TYPE=Release ..
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

echo [INFO] Compiling...
%CMAKE_EXE% --build . --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed.
    exit /b 1
)
cd ..

echo [INFO] Deploying application...
mkdir bin

:: 复制 exe
if not exist build\APEHOI4ToolStudio.exe (
    echo [ERROR] Executable not found in build directory.
    exit /b 1
)
copy build\APEHOI4ToolStudio.exe bin\
if %errorlevel% neq 0 (
    echo [ERROR] Failed to copy executable.
    exit /b 1
)

:: 复制核心库
if exist build\libAPEHOI4Core.dll (
    copy build\libAPEHOI4Core.dll bin\
) else if exist build\APEHOI4Core.dll (
    copy build\APEHOI4Core.dll bin\
) else (
    echo [WARNING] Core library DLL not found in build directory.
)

:: 复制工具目录
if exist build\tools (
    echo [INFO] Copying tools...
    xcopy /E /I /Y build\tools bin\tools
)

:: 运行 windeployqt
echo [INFO] Running windeployqt to copy dependencies...
windeployqt --release --compiler-runtime bin\APEHOI4ToolStudio.exe
if %errorlevel% neq 0 (
    echo [ERROR] windeployqt failed.
    exit /b 1
)

echo.
echo [SUCCESS] Deployment complete!
echo.
echo Creating shortcut...

:: 创建快捷方式
set SCRIPT="%TEMP%\CreateShortcut.vbs"
echo Set oWS = WScript.CreateObject("WScript.Shell") > %SCRIPT%
echo sLinkFile = "%CD%\APE HOI4 Tool Studio.lnk" >> %SCRIPT%
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> %SCRIPT%
echo oLink.TargetPath = "%CD%\bin\APEHOI4ToolStudio.exe" >> %SCRIPT%
echo oLink.WorkingDirectory = "%CD%\bin" >> %SCRIPT%
echo oLink.Save >> %SCRIPT%

cscript /nologo %SCRIPT%
del %SCRIPT%

echo [DONE] Shortcut created.
:: pause
