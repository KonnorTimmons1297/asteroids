@echo off

setlocal

IF EXIST maskeroids.exe del maskeroids.exe
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

set LIBS=libvcruntime.lib kernel32.lib gdi32.lib user32.lib ucrt.lib d3d11.lib dxguid.lib
cl ..\maskeroids.c /nologo /W3 /WX /GS- /MTd /Od /Zi /Fe"maskeroids" /link /fixed /incremental:no /opt:icf /opt:ref /subsystem:windows %LIBS%
move maskeroids.exe ..
popd

endlocal