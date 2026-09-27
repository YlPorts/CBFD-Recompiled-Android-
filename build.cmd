@echo off
rem Builds Conker's Bad Fur Day: Recompiled on Windows from a clone and your ROM:
rem   build.cmd [path\to\rom.z64]
rem The ROM is copied to conker\baserom.us.z64 (needed once). Needs Git, Python 3 and
rem Visual Studio with C++; nothing else. Safe to run again after `git pull`: only what
rem changed is rebuilt.
setlocal EnableExtensions
cd /d "%~dp0"
set "ROOT=%CD%"

echo "%ROOT%" | findstr /c:"'" >nul && (
    echo Error: the folder path contains an apostrophe ^(%ROOT%^). Some of RT64's build
    echo steps break on one: move the clone somewhere else.
    exit /b 1
)

echo.
echo ==^> Checking the tools
where git >nul 2>&1 || (
    echo Error: Git for Windows wasn't found. Install it from https://git-scm.com/download/win
    exit /b 1
)
rem Python: the py launcher if there is one; a plain `python` can be the Microsoft Store's
rem placeholder, which only opens the Store, so it has to run to count.
set "PY="
py -3 --version >nul 2>&1 && set "PY=py -3"
if not defined PY python --version >nul 2>&1 && set "PY=python"
if not defined PY (
    echo Error: Python 3 wasn't found. Install it from https://www.python.org/downloads/
    echo ^(tick "Add python.exe to PATH" in the installer^), then run this again.
    exit /b 1
)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo Error: Visual Studio with C++ wasn't found. Install Visual Studio 2022 or later, or
    echo the free Build Tools for Visual Studio, with the "Desktop development with C++"
    echo workload: https://visualstudio.microsoft.com/downloads/
    exit /b 1
)

if not "%~1"=="" (
    if not exist "%~1" (
        echo Error: no file at %~1
        exit /b 1
    )
    echo.
    echo ==^> Copying your ROM to conker\baserom.us.z64
    copy /y "%~1" conker\baserom.us.z64 >nul || exit /b 1
)
if not exist conker\baserom.us.z64 (
    echo Error: no ROM yet. Run this with the path to your ROM, for example:
    echo   build.cmd "%USERPROFILE%\Downloads\conker.z64"
    exit /b 1
)

echo.
echo ==^> Getting the submodules
rem A pull can move a patched tool to another commit, which its patch would block.
call :reset_if_moved tools/N64Recomp
call :reset_if_moved tools/N64ModernRuntime
call :reset_if_moved tools/rt64
git submodule update --init --recursive || exit /b 1

echo.
echo ==^> Patching the tools
call :apply_patch tools/N64Recomp recomp/n64recomp.patch || exit /b 1
call :apply_patch tools/N64ModernRuntime recomp/n64modernruntime.patch || exit /b 1
call :apply_patch tools/rt64 recomp/rt64.patch || exit /b 1

echo.
echo ==^> Building the recompiler
setlocal
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist tools\N64Recomp\build-win\build.ninja (
    cmake -S tools/N64Recomp -B tools/N64Recomp/build-win -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
)
cmake --build tools/N64Recomp/build-win --target N64RecompCLI RSPRecomp || exit /b 1
endlocal

echo.
echo ==^> Recompiling the game from your ROM
%PY% recomp\recompile.py --bin tools/N64Recomp/build-win || exit /b 1

echo.
echo ==^> Building the game
call host\build_windows.cmd || exit /b 1

echo.
echo Done. Run the game: %ROOT%\host\build-win\ConkerRecomp.exe
echo The first time, pick Load ROM in the launcher and select your ROM
echo (conker\baserom.us.z64 works).
exit /b 0

rem Applies a patch unless it's already applied. If the tool holds an older version of
rem the patch (after a git pull that changed it), the tool is reset and patched again.
:apply_patch
git -C "%~1" apply --reverse --check "%ROOT%/%~2" >nul 2>&1 && exit /b 0
git -C "%~1" apply --check "%ROOT%/%~2" >nul 2>&1 || (
    echo   %~1 has changes that aren't %~2 ^(probably an older version of it^): resetting it.
    git -C "%~1" reset --hard -q
    git -C "%~1" clean -fdq
)
git -C "%~1" apply "%ROOT%/%~2" || (
    echo Error: couldn't apply %~2 to %~1.
    exit /b 1
)
echo   patched %~1
exit /b 0

:reset_if_moved
if not exist "%~1/.git" exit /b 0
set "WANT="
set "HAVE="
for /f "tokens=3" %%c in ('git ls-tree HEAD %~1') do set "WANT=%%c"
for /f %%c in ('git -C "%~1" rev-parse HEAD') do set "HAVE=%%c"
if not "%WANT%"=="%HAVE%" (
    echo   %~1 moved to another commit: resetting it ^(its patch is applied again below^).
    git -C "%~1" reset --hard -q
    git -C "%~1" clean -fdq
)
exit /b 0
