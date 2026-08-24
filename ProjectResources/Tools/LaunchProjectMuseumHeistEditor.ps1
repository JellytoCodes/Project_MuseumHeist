param(
    [Parameter(Position = 0, ValueFromRemainingArguments)]
    [string[]] $ExtraArgs
)

$ProjectRoot = "D:\Dev\UE5.8\Project_MuseumHeist"
$UnrealEditor = "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $ProjectRoot "Project_MuseumHeist.uproject"
$LocalDdcPath = Join-Path $ProjectRoot "Saved\DerivedDataCache"

New-Item -ItemType Directory -Path $LocalDdcPath -Force | Out-Null

& $UnrealEditor $Project -DDC-ForceMemoryCache "-LocalDataCachePath=$LocalDdcPath" -NoSplash -log @ExtraArgs
