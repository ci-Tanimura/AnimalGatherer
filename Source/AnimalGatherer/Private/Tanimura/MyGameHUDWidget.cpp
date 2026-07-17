// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MyGameHUDWidget.h"
#include "Tanimura/MainGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

void UMyGameHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // MainGameModeを取得
    if (AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(GetWorld()))) {
        // 残り時間変更イベントのバインド
        GameMode->OnTimeChanged.AddDynamic(this, &UMyGameHUDWidget::UpdateTimerText);

        // スコア変更イベントのバインド
        GameMode->OnScoreChanged.AddDynamic(this, &UMyGameHUDWidget::UpdateScoreText);

        // タイムアップイベントのバインド
        GameMode->OnTimeUp.AddDynamic(this, &UMyGameHUDWidget::PlayTimeUpSequence);

        // ゲーム開始時の最初のスコア（0対0）をUIに反映しておく
        UpdateScoreText(0, 0);
    }
}