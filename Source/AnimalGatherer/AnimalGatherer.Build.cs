// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimalGatherer : ModuleRules
{
	public AnimalGatherer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// 2026.10.06 Lee start（技能システムの HUD 実装向けに UMG を公開依存へ追加）
		// PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ApplicationCore" }); ←元のコードは消さない
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ApplicationCore", "UMG" });
		// 2026.10.06 Lee end（技能システムの HUD 実装向けに UMG を公開依存へ追加）

		// 2026.10.16 Lee start（ゴール演出の Niagara 再生向けに Niagara を公開依存へ追加）
		PublicDependencyModuleNames.AddRange(new string[] { "Niagara" });
		// 2026.10.16 Lee end（ゴール演出の Niagara 再生向けに Niagara を公開依存へ追加）

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
