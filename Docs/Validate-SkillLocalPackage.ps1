param([ValidatePattern('^[a-zA-Z0-9-]+$')][string]$RunName=(Get-Date -Format 'yyyyMMdd-HHmmss'))
$ErrorActionPreference='Stop'
$skillValidationRoot="C:/unreal/AnimalGatherer/Saved/SkillEditorValidation/$RunName"
if(Test-Path -LiteralPath $skillValidationRoot) { throw 'Validation directory already exists; choose a new RunName to preserve previous output' }
New-Item -ItemType Directory -Path $skillValidationRoot | Out-Null
$skillUat='C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat'
$skillCommon=@('BuildCookRun','-noP4','-project=C:/unreal/AnimalGatherer/AnimalGatherer.uproject','-platform=Win64','-clientconfig=Development','-skipbuild','-nocompileeditor','-unattended','-utf8output')
& $skillUat @skillCommon -cook '-map=/Game/Maps/LV_Title+/Game/Maps/LV_Tutorial+/Game/Maps/LV_MainGame+/Game/Maps/LV_Result' "-CookOutputDir=$skillValidationRoot/Cooked/Windows" 2>&1 | Tee-Object -FilePath "$skillValidationRoot/Cook.log" | ForEach-Object { if($_ -match 'ERROR:|Error:|BUILD SUCCESSFUL|COOK COMMAND COMPLETED|AutomationTool exiting|ExitCode=') { $_ } }
$skillCookExit=$LASTEXITCODE
if($skillCookExit -ne 0) { exit $skillCookExit }
& $skillUat @skillCommon -skipcook -stage -pak -archive "-CookOutputDir=$skillValidationRoot/Cooked" "-stagingdirectory=$skillValidationRoot/Staged" "-archivedirectory=$skillValidationRoot/WindowsBuild" 2>&1 | Tee-Object -FilePath "$skillValidationRoot/Package.log" | ForEach-Object { if($_ -match 'ERROR:|Error:|BUILD SUCCESSFUL|STAGE COMMAND COMPLETED|PAK COMMAND COMPLETED|ARCHIVE COMMAND COMPLETED|AutomationTool exiting|ExitCode=') { $_ } }
$skillPackageExit=$LASTEXITCODE
"Packaging exit code: $skillPackageExit"
exit $skillPackageExit
