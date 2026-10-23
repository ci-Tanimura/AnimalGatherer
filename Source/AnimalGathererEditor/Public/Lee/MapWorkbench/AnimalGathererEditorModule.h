// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

// 前方宣言（本ヘッダー単独での解決用：SDockTab.h / TabManager.h は実装側で include する）
class SDockTab;
class FSpawnTabArgs;

// 2026.10.22 Lee start（マップ工作台 Editor モジュール：骨格のみ。正式パネルは後続バッチで実装）

/**
 * @brief マップ工作台タブの識別子を返す。
 *        Nomad Tab スポナー登録と Window メニュー入口で共通して使用する。
 * @return タブ識別子（"MapWorkbench"）。
 */
ANIMALGATHEREREDITOR_API FName GetMapWorkbenchTabName();

/**
 * @brief 地图工作台用の独立 Editor モジュール。
 *        Window メニュー入口と Nomad タブの登録・解除のみを担う。
 */
class ANIMALGATHEREREDITOR_API FAnimalGathererEditorModule : public IModuleInterface
{
public:
	/** @brief モジュール起動処理。Nomad Tab スポナーと ToolMenus 起動コールバックを登録する。 */
	virtual void StartupModule() override;

	/** @brief モジュール終了処理。メニューコールバック・オーナー・タブスポナーを解除する。 */
	virtual void ShutdownModule() override;

private:
	/** @brief ToolMenus 起動完了後に Window メニューへ「地图工作台」入口を追加する。 */
	void RegisterMenus();

	/** @brief Nomad タブ生成時にワークベンチ骨格ウィジェットを構築する。 */
	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& Args);

	/** @brief Window メニュー入口からワークベンチタブを呼び出す。 */
	void InvokeWorkbenchTab();

	/** @brief UToolMenus 起動コールバックのハンドル（解除用に保持）。未登録時は無効。 */
	FDelegateHandle ToolMenusStartupHandle;
};
// 2026.10.22 Lee end
