// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/MyGameHUDWidget.h"
#include "Tanimura/MainGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

void UMyGameHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// MainGameModeを取得
	AMainGameMode* GameMode = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GameMode) {
		UE_LOG(LogTemp, Warning, TEXT("UMyGameHUDWidget::NativeConstructで、AMainGameModeを取得できませんでした。"));
		return;
	}
	// スコア変更イベントのバインド
	GameMode->OnTimeChanged.AddUniqueDynamic(this, &UMyGameHUDWidget::UpdateTimerText);
	GameMode->OnScoreChanged.AddUniqueDynamic(this, &UMyGameHUDWidget::UpdateScoreText);
	GameMode->OnTimeUp.AddUniqueDynamic(this, &UMyGameHUDWidget::PlayTimeUpSequence);
}