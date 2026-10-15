# PROTOTYPE – startet einen Messlauf des Async-Physik-Spikes (Issue #2) headless und zeigt die SPIKE-Zeilen.
# Beispiel: .\RunSpike.ps1 -StepSize 0.008333 -SleepMs 0
param(
    [double]$StepSize = 0.0083333,
    [double]$MaxPhysicsDeltaTime = 0.0333333,
    [double]$SleepMs = 0,
    [double]$MaxFps = 0,
    [double]$Duration = 12,
    [switch]$Sync,
    [string]$Extra = ""   # CVars ab Start (vor BeginPlay), z. B. "spike.DisableSleep=1,spike.Accel=1000"
)

$Engine = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Resolve-Path "$PSScriptRoot\..\..\..\GTO.uproject"
$Log = Join-Path $env:TEMP ("spike-{0}.log" -f [guid]::NewGuid().ToString("N").Substring(0, 8))
$Async = if ($Sync) { "False" } else { "True" }
$S = "[/Script/Engine.PhysicsSettings]"
$Ini = "-ini:Engine:${S}:bTickPhysicsAsync=$Async,${S}:AsyncFixedTimeStepSize=$StepSize,${S}:MaxPhysicsDeltaTime=$MaxPhysicsDeltaTime"
$Exec = "spike.GameThreadSleepMs $SleepMs,spike.DurationSec $Duration,t.MaxFPS $MaxFps"
$CVars = if ($Extra) { "-DPCVars=$Extra" } else { "" }

& $Engine $Project "/Engine/Maps/Entry?game=/Script/GTO.SpikeGameMode" -game -nullrhi -nosound -unattended -nosplash `
    $Ini "-ExecCmds=$Exec" $CVars "-abslog=$Log" | Out-Null

Select-String -Path $Log -Pattern "SPIKE|Assertion|Ensure|LogPhysics: Error|LogChaos.*Error" | ForEach-Object { $_.Line -replace '^\[.*?\]\[.*?\]', '' }
