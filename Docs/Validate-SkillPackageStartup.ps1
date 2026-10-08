$ErrorActionPreference='Stop'
$skillPackagedExe='C:/unreal/AnimalGatherer/Saved/SkillEditorValidation/20261008/WindowsBuildFinal/AnimalGatherer/Binaries/Win64/AnimalGatherer.exe'
if(!(Test-Path -LiteralPath $skillPackagedExe)) { throw 'Packaged executable missing' }
$skillSmokeResults=@()
foreach($skillMapName in @('LV_Title','LV_Tutorial','LV_MainGame','LV_Result')) {
    $skillSmokeLog="C:/unreal/AnimalGatherer/Saved/SkillEditorValidation/20261008/Smoke-$skillMapName.log"
    $skillSmokeArgs=@("/Game/Maps/$skillMapName",'-nullrhi','-RenderOffScreen','-unattended','-nosound',"-abslog=$skillSmokeLog",'-ExecCmds="getall SkillSystemComponent Slots,quit"')
    $skillSmokeProcess=Start-Process -FilePath $skillPackagedExe -ArgumentList $skillSmokeArgs -WorkingDirectory (Split-Path $skillPackagedExe) -WindowStyle Hidden -PassThru
    $skillSmokeTimer=[Diagnostics.Stopwatch]::StartNew()
    while(!$skillSmokeProcess.HasExited -and $skillSmokeTimer.Elapsed.TotalSeconds -lt 45) { Start-Sleep -Milliseconds 500; $skillSmokeProcess.Refresh() }
    $skillSmokeTimedOut=!$skillSmokeProcess.HasExited
    if($skillSmokeTimedOut) { Stop-Process -Id $skillSmokeProcess.Id; $skillSmokeProcess.WaitForExit() }
    $skillSmokeExit=$skillSmokeProcess.ExitCode
    $skillSmokeText=if(Test-Path -LiteralPath $skillSmokeLog) { Get-Content -LiteralPath $skillSmokeLog -Raw } else { '' }
    $skillSmokeLoaded=$skillSmokeText -match "(LoadMap: /Game/Maps/$skillMapName|Bringing World /Game/Maps/$skillMapName|Took .*LoadMap\(/Game/Maps/$skillMapName)"
    $skillSmokeBad=$skillSmokeText -match 'Fatal error:|Assertion failed:|LogBlueprint: Error:|LogLinker: Error:|Failed to find object'
    $skillSmokeResults += [pscustomobject]@{map=$skillMapName;exitCode=$skillSmokeExit;timedOut=$skillSmokeTimedOut;loaded=$skillSmokeLoaded;errorFound=$skillSmokeBad;seconds=[math]::Round($skillSmokeTimer.Elapsed.TotalSeconds,2);log=$skillSmokeLog;method='NullRHI startup, deferred quit'}
    $skillSmokeResults[-1] | ConvertTo-Json -Compress
}
$skillSmokeResults | ConvertTo-Json -Depth 8 | Set-Content 'C:/unreal/AnimalGatherer/Saved/SkillEditorValidation/20261008/PackageSmokeResults.json'
if(@($skillSmokeResults | Where-Object { $_.exitCode -ne 0 -or $_.timedOut -or !$_.loaded -or $_.errorFound }).Count) { exit 1 }
