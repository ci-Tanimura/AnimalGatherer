// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/MapWorkbench/AnimalGathererEditorModule.h"

// 2026.10.22 Lee start（マップ工作台 Editor モジュール実装：タブとメニュー登録のみ）

#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "Lee/MapWorkbench/MapWorkbenchText.h"
#include "Lee/MapWorkbench/SMapWorkbench.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

FName GetMapWorkbenchTabName()
{
	static const FName MapWorkbenchTabId(TEXT("MapWorkbench"));
	return MapWorkbenchTabId;
}

void FAnimalGathererEditorModule::StartupModule()
{
	// ワークベンチ用の Nomad タブスポナーを登録する（単一タブで動く常駐ツール）
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		GetMapWorkbenchTabName(),
		FOnSpawnTab::CreateRaw(this, &FAnimalGathererEditorModule::OnSpawnPluginTab))
		// 2026.10.23 Lee start（表示時に言語を解決するキャプチャなしラムダ属性へ置換：登録時に固定されていた旧実装はコメントで残留）
		.SetDisplayNameAttribute(TAttribute<FText>::CreateLambda([]() -> FText
		{
			return FMapWorkbenchText::Get(EWorkbenchText::MenuTitle);
		}))
		//.SetDisplayName(FMapWorkbenchText::Get(EWorkbenchText::MenuTitle))←元のコードは消さない
		// 2026.10.23 Lee end
		.SetMenuType(ETabSpawnerMenuType::Enabled);

	// エディタ起動順序に依存しないよう、ToolMenus の初期化完了後にメニューを拡張する
	ToolMenusStartupHandle = UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAnimalGathererEditorModule::RegisterMenus));
}

void FAnimalGathererEditorModule::ShutdownModule()
{
	// 未実行の起動コールバックを解除する（登録済みの場合のみ）
	if (ToolMenusStartupHandle.IsValid())
	{
		UToolMenus::UnRegisterStartupCallback(ToolMenusStartupHandle);
		ToolMenusStartupHandle.Reset();
	}

	// 本モジュールをオーナーとするメニュー拡張を全て解除する
	UToolMenus::UnregisterOwner(this);

	// 登録した Nomad タブスポナーを解除する
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GetMapWorkbenchTabName());
}

void FAnimalGathererEditorModule::RegisterMenus()
{
	// メニュー拡張のオーナーを明示して登録する（ShutdownModule の UnregisterOwner と対になる）
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
	Section.AddMenuEntry(
		"MapWorkbench",
		// 2026.10.23 Lee start（表示時に言語を解決するキャプチャなしラムダ属性へ置換：即時求値だった旧実装はコメントで残留）
		TAttribute<FText>::CreateLambda([]() -> FText
		{
			return FMapWorkbenchText::Get(EWorkbenchText::MenuTitle);
		}),
		TAttribute<FText>::CreateLambda([]() -> FText
		{
			return FMapWorkbenchText::Get(EWorkbenchText::MenuTooltip);
		}),
		//FMapWorkbenchText::Get(EWorkbenchText::MenuTitle),←元のコードは消さない
		//FMapWorkbenchText::Get(EWorkbenchText::MenuTooltip),←元のコードは消さない
		// 2026.10.23 Lee end
		FSlateIcon(),
		FToolUIActionChoice(FUIAction(FExecuteAction::CreateRaw(this, &FAnimalGathererEditorModule::InvokeWorkbenchTab))));
}

TSharedRef<SDockTab> FAnimalGathererEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& Args)
{
	// 2026.10.22 Lee start（仮骨格から正式なワークベンチパネルへ差し替え）
	// 旧：SNew(SVerticalBox) + STextBlock「地图工作台面板接入中」の仮骨格 ←元のコードは消さない
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SMapWorkbench)
		];
	// 2026.10.22 Lee end
}

void FAnimalGathererEditorModule::InvokeWorkbenchTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(GetMapWorkbenchTabName());
}

IMPLEMENT_MODULE(FAnimalGathererEditorModule, AnimalGathererEditor)
// 2026.10.22 Lee end

// 2026.10.23 Lee start（元コードの集中留存）
/**
 * @brief 言語対応改修で置換された元コードのコメント留存。
 *        有効なコードではない。
 */
//		.SetDisplayName(FText::FromString(TEXT("地图工作台")))
//		FText::FromString(TEXT("地图工作台")),
//		FText::FromString(TEXT("打开地图工作台面板（选中一个 MapManager 后使用）")),
// 2026.10.23 Lee end
// PRESERVATION-MARKER: MAPWORKBENCH-PRESERVED-ORIGINAL-MODULE-2026.10.23-LEE
