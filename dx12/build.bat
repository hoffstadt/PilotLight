@REM --------------------------------------------------------------------------
@REM Setup
@REM --------------------------------------------------------------------------
@setlocal
@set dir=%~dp0
@pushd %dir%
@set PATH=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set OUT_DIR=out
@set OUT_BIN=d3d12_example.exe

@REM --------------------------------------------------------------------------
@REM Include paths
@REM --------------------------------------------------------------------------
@set INCLUDES=/I. /I "%WindowsSdkDir%Include\um" /I "%WindowsSdkDir%Include\shared" /I "%DXSDK_DIR%Include"

@REM --------------------------------------------------------------------------
@REM Sources
@REM --------------------------------------------------------------------------
@set SOURCES=main.cpp

@REM Remove old files
@IF NOT EXIST %OUT_DIR% mkdir %OUT_DIR%
@IF EXIST %OUT_DIR%\%OUT_BIN% del %OUT_DIR%\%OUT_BIN%

@REM Setup development environment
@call vcvarsall.bat amd64

@REM --------------------------------------------------------------------------
@REM Compiler flags
@REM --------------------------------------------------------------------------
set CommonCompilerFlags=/nologo /Zi /MD /std:c++17
set CommonCompilerFlags=/D _USE_MATH_DEFINES /D _DEBUG /D MV_DEBUG %CommonCompilerFlags% 

@REM --------------------------------------------------------------------------
@REM Linker flags
@REM --------------------------------------------------------------------------
set CommonLinkerFlags=/LIBPATH:"%DXSDK_DIR%/Lib/x86" d3d12.lib d3dcompiler.lib
set CommonLinkerFlags=ucrtd.lib gdi32.lib user32.lib %CommonLinkerFlags%
set CommonLinkerFlags=comctl32.lib Shell32.lib dxguid.lib dxgi.lib %CommonLinkerFlags%

@REM --------------------------------------------------------------------------
@REM Build
@REM --------------------------------------------------------------------------
cl %CommonCompilerFlags% %INCLUDES% %SOURCES% /Fe%OUT_DIR%/%OUT_BIN% /Fo%OUT_DIR%/ /link %CommonLinkerFlags%

@REM --------------------------------------------------------------------------
@REM Cleanup
@REM --------------------------------------------------------------------------
del %OUT_DIR%\*.obj
@popd
@endlocal