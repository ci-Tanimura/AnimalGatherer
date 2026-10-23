// Copyright Epic Games, Inc. All Rights Reserved.

// 2026.10.22 Lee start（マップ工作台用の独立 Editor モジュール構築規則：依存はプロジェクト規約に従う）
using UnrealBuildTool;

public class AnimalGathererEditor : ModuleRules
{
	public AnimalGathererEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 盤面データ（AMapManager / AAnimalSpawner / GameTypes）を参照するため Runtime モジュールへ依存する
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AnimalGatherer"
		});

		// Slate 面板 / UnrealEd 事务 / ToolMenus 菜单 / 素材选择器 / 属性编辑 に必要な Editor 系モジュール
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"ContentBrowser",
			"AssetRegistry",
			"PropertyEditor"
		});
	}
}
// 2026.10.22 Lee end
