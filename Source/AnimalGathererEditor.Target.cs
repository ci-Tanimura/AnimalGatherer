// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class AnimalGathererEditorTarget : TargetRules
{
	public AnimalGathererEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("AnimalGatherer");
		// 2026.10.22 Lee start（マップ工作台 Editor モジュールを Editor ターゲットへ追加）
		ExtraModuleNames.Add("AnimalGathererEditor");
		// 2026.10.22 Lee end
	}
}
