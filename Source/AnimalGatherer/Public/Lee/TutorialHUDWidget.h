// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Lee/TutorialGameMode.h"
#include "TutorialHUDWidget.generated.h"

/**
 * @brief チュートリアル用 HUD の C++ 基底クラス。
 *        UMyGameHUDWidget と同じ構成：C++ はデリゲート購読のみ行い、
 *        実際の UI 更新はブループリント側の BlueprintImplementableEvent で実装する。
 */
UCLASS()
class ANIMALGATHERER_API UTutorialHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ── デリゲート受信ハンドラ ──

	/** @brief ステップ変化の受信。 */
	UFUNCTION()
	void HandleStepChanged(ETutorialStep NewStep);

	/** @brief プレイヤー達成状態変化の受信。 */
	UFUNCTION()
	void HandlePlayerDone(uint8 PlayerID, bool bDone);

	/** @brief チュートリアル完了の受信。 */
	UFUNCTION()
	void HandleComplete();

	// ── 実装はブループリント側（WBP_Tutorial） ──

	/** @brief ステップ表示を更新する（説明文の切替 + 両プレイヤーの達成マーク消去）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Update")
	void ShowStep(ETutorialStep Step);

	/** @brief プレイヤーの達成マークを更新する（0 = 1P 青 / 1 = 2P 赤）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Update")
	void SetPlayerReady(uint8 PlayerID, bool bReady);

	/** @brief チュートリアル完了演出を表示する。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial|Update")
	void ShowComplete();
};
