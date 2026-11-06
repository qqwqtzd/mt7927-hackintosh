@echo off
rem =====================================================================
rem  MtkWlanWiFi recovery: disable the WiFi/BT kexts (recover the boot)
rem  Run as Administrator. Safe to run twice. Only renames directories,
rem  never touches config.plist.
rem =====================================================================
setlocal EnableDelayedExpansion

set "FOUND_OC="
for %%D in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    if exist "%%D:\EFI\OC\OpenCore.efi" (
        set "FOUND_OC=%%D:\EFI\OC"
    )
)

if not defined FOUND_OC (
    echo ERROR: no mounted EFI partition with EFI\OC was found on C-Z.
    echo Use diskpart to assign drive letters first (see RECOVERY-MTKWLAN.txt): 
    echo     diskpart
    echo     list disk
    echo     select disk N^<the Mac disk^>
    echo     select partition 1
    echo     assign letter=Z
    echo     exit
    echo Then run this script again.
    exit /b 1
)

echo Found OpenCore at: %FOUND_OC%
set "K=%FOUND_OC%\Kexts"

set "DONE=0"
for %%K in (MtkWlan MtkWlanNative MtkBt BlueToolFixup LegionBtFix) do (
    if exist "%K%\%%K.kext" (
        if not exist "%K%\%%K.kext.off" (
            move "%K%\%%K.kext" "%K%\%%K.kext.off" >nul
            echo disabled %%K.kext
            set "DONE=1"
        ) else (
            echo %%K.kext already off
        )
    )
)
if "%DONE%"=="1" (
    echo.
    echo All WiFi/BT kexts are now disabled. The Mac will boot without WiFi/BT.
    echo After the boot, in macOS run:  bash tools/restore-kexts.sh
    echo (or ask the assistant to restore the previous build).
) else (
    echo Nothing to disable - all were already off.
)
exit /b 0
