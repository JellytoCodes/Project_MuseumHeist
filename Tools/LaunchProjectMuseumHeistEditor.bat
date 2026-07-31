@echo off
setlocal

set "UE_EDITOR=D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "UPROJECT=D:\Dev\UE5.8\Project_MuseumHeist\Project_MuseumHeist.uproject"
set "DDC_ROOT=D:\Dev\UE5.8\Project_MuseumHeist\Saved\DerivedDataCache"

if not exist "%DDC_ROOT%" mkdir "%DDC_ROOT%"

"%UE_EDITOR%" "%UPROJECT%" -DDC-ForceMemoryCache -LocalDataCachePath="%DDC_ROOT%" -NoSplash -log

endlocal
