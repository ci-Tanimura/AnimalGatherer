// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyGameHUDWidget.generated.h"

/**
 * ゲーム本編のHUDウィジェット
 * MainGameModeのイベントを購読し、BlueprintImplementableEvent経由でWBP側の表示更新を呼び出す
 * 購読と中継のみを担い、見た目の構築はWBPが担う
 */
UCLASS()
class ANIMALGATHERER_API UMyGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	// ウィジェットが階層に追加されるたびに呼ばれる
	virtual void NativeConstruct() override;

	// 残り時間が変わったらテキストを更新するイベント
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Update")
	void UpdateTimerText(int32 RemainingTime);

	// スコアが変わったらテキストを更新するイベント
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Update")
	void UpdateScoreText(int32 P1Score, int32 P2Score);

	// タイムアップ時にゲーム終了演出を開始するイベント
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Update")
	void PlayTimeUpSequence();
};