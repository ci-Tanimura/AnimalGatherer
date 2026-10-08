param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [int]$MaxParallelActions = 2
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'AnimalGatherer.uproject'
$buildScript = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$logDirectory = Join-Path $projectRoot 'Saved\Logs'

if (!(Test-Path -LiteralPath $buildScript) -or !(Test-Path -LiteralPath $projectFile)) {
    throw 'Engine build script or project file is missing.'
}
if (!(Test-Path -LiteralPath $logDirectory)) {
    New-Item -ItemType Directory -Path $logDirectory | Out-Null
}

Push-Location -LiteralPath $projectRoot
try {
    & git -c core.safecrlf=false diff --check
    if ($LASTEXITCODE -ne 0) { throw 'Git diff whitespace validation failed.' }

    $baseline = Get-Content -LiteralPath (Join-Path $PSScriptRoot '技能代码阶段资产基线.md') -Raw | ConvertFrom-Json
    foreach ($entry in $baseline) {
        if ((Get-FileHash -LiteralPath $entry.Path -Algorithm SHA256).Hash -ne $entry.Hash) {
            throw "Asset baseline changed: $($entry.Path)"
        }
    }

    foreach ($target in @('AnimalGathererEditor', 'AnimalGatherer')) {
        $logPath = Join-Path $logDirectory "SkillSystem-$target-Build.log"
        & $buildScript $target Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE "-MaxParallelActions=$MaxParallelActions" 2>&1 |
            Tee-Object -FilePath $logPath
        $buildExitCode = $LASTEXITCODE
        if ($buildExitCode -ne 0) {
            throw "Build failed: $target, exit code $buildExitCode. Log: $logPath"
        }
    }
} finally {
    Pop-Location
}
