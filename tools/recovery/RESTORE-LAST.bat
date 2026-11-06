@echo off
rem =====================================================================
rem  Restore the previous MtkWlan build (rollback one version).
rem  Run as Administrator. Only restores MtkWlanNex from the newest
rem  MtkWlanNex.kext.before-* directory it finds; other kexts untouched.
rem =====================================================================
setlocal EnableDelayedExpansion

set "FOUND_OC="
for %%D in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    if exist "%%D:\EFI\OC\OpenCore.efi" set "FOUND_OC=%%D:\EFI\OC"
)
if not defined FOUND_OC (
    echo ERROR: no mounted EFI\OC found on C-Z (assign drive letters first, see RECOVERY-MTKWLAN.txt)
    exit /b 1
)
set "K=%FOUND_OC%\Kexts"
echo OpenCore at: %FOUND_OC%

rem newest backup of MtkWlan
set "NEW="
for /f "delims=" %%d in ('dir /b /ad /o-d "%K%\MtkWlan.kext.before-*" 2^>nul') do (
    if not defined NEW set "NEW=%%d"
)
if not defined NEW (
    echo No MtkWlan.kext.before-* backup found - nothing to restore.
    exit /b 1
)
if exist "%K%\MtkWlan.kext.off" (
    move "%K%\MtkWlan.kext.off" "%K%\MtkWlan.kext" >nul
    echo re-enabled MtkWlan.kext from .off
)
echo restoring %NEW%
rmdir /s /q "%K%\MtkWlan.kext" 2>nul
move "%K%\%NEW%" "%K%\MtkWlan.kext" >nul
echo restored. Full list of backups:
dir /b "%K%\MtkWlan.kext.before-*" 2>nul
echo.
echo To roll back further, delete MtkWlan.kext and rename an older before-* dir back.
exit /b 0
