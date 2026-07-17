// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MyGameHUDWidget.generated.h"

/**
 * 
 */
UCLASS()
class ANIMALGATHERER_API UMyGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
    // ウィジェットのコンストラクタ
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