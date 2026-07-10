// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyGameWidget.generated.h"

/**
 * 
 */
UCLASS()
class ANIMALGATHERER_API UMyGameWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
    // ウィジェット生成時に一度だけ呼ばれる初期化関数
    virtual void NativeConstruct() override;

    // 残り時間が変わったときにUIのテキストを更新するイベント
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Update")
    void UpdateTimerText(int32 RemainingTime);

    // スコアが変わったときにUIのテキストを更新するイベント
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Update")
    void UpdateScoreText(int32 P1Score, int32 P2Score);

    // タイムアップ時にゲームオーバー演出（演出アニメ等）を開始するイベント
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Update")
    void PlayTimeUpSequence();
};
