$ErrorActionPreference='Stop'
$skillPackageLog='C:/unreal/AnimalGatherer/Saved/SkillEditorValidation/20261008/Package.log'
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat' BuildCookRun -noP4 '-project=C:/unreal/AnimalGatherer/AnimalGatherer.uproject' -platform=Win64 -clientconfig=Development -skipbuild -nocompileeditor -cook -stage -pak -archive '-map=/Game/Maps/LV_Title+/Game/Maps/LV_Tutorial+/Game/Maps/LV_MainGame+/Game/Maps/LV_Result' '-CookOutputDir=C:/unreal/AnimalGatherer/Saved/SkillEditorValidation/20261008/Cooked' '-stagingdirectory=C:/unreal/AnimalGatherer/Saved/SkillEditorValidation/20261008/Staged' '-archivedirectory=C:/unreal/AnimalGatherer/Saved/SkillEditorValidation/20261008/WindowsBuild' -unattended -utf8output 2>&1 | Tee-Object -FilePath $skillPackageLog | ForEach-Object { if($_ -match 'ERROR:|Error:|BUILD SUCCESSFUL|COOK COMMAND COMPLETED|STAGE COMMAND COMPLETED|PAK COMMAND COMPLETED|ARCHIVE COMMAND COMPLETED|AutomationTool exiting|ExitCode=|Running AutomationTool|Cooked packages [0-9]') { $_ } }
$skillPackageExit=$LASTEXITCODE
"Packaging exit code: $skillPackageExit"
exit $skillPackageExit
