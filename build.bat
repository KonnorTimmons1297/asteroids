@echo off

setlocal

IF EXIST asteroids.exe del asteroids.exe
IF NOT EXIST build mkdir build

pushd build
REM === COMPILER OPTIONS ===
REM /nologo - don't display compiler information
REM /W3 /WX - Warnings
REM /GS- - don't add buffer check code that requires CRT
REM /O1 or /O2 - Optimizations
REM /Od - Disable Optimizations
REM /Zi - produce debug info generations(passes /debug to linker)
REM /RTC1 - produce extra run-time error checks for variables and arrays on stack
REM === LINKER OPTIONS ===
REM /link - Specify Linker options
REM /fixed - don't add relocation section to .exe(don't use when building .dll)
REM /incremental:no - don't do incremental linking with .pdb file because it gets corrupted often
REM /opt:icf - perform identical code folding, in case multiple functions generates identical code bytes
REM /opt:ref - remove unreferenced functions/globals
REM /subsystem:windows - create "gui" executable without console attached

IF "%1" == "release" ( 
    echo ===== Building Release Executable =====
    set CompilerFlags=/nologo /W4 /WX /GS- /MT /Oi /O2 /FC
) ELSE (
    echo ===== Building Debug Executable =====
    REM /fsanitize=address
    set CompilerFlags=/nologo /W4 /WX /GS- /MTd /Zi /DDEBUG
)

set LIBS=kernel32.lib user32.lib ucrt.lib d3d11.lib dxguid.lib dxgi.lib
cl ..\asteroids.c %CompilerFlags% /Fe"asteroids" /link /fixed /incremental:no /opt:icf /opt:ref /subsystem:windows %LIBS%
move asteroids.exe ..
popd

endlocal