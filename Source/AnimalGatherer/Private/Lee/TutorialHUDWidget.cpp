// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/TutorialHUDWidget.h"
#include "Lee/TutorialGameMode.h"
#include "Kismet/GameplayStatics.h"

void UTutorialHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// TutorialGameMode を取得してデリゲートを購読する
	if (ATutorialGameMode* GameMode = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		GameMode->OnTutorialStepChanged.AddDynamic(this, &UTutorialHUDWidget::HandleStepChanged);
		GameMode->OnTutorialPlayerDone.AddDynamic(this, &UTutorialHUDWidget::HandlePlayerDone);
		GameMode->OnTutorialComplete.AddDynamic(this, &UTutorialHUDWidget::HandleComplete);

		// 購読開始時点のステップを反映（遅延生成対応）
		ShowStep(GameMode->GetCurrentStep());
	}
}

void UTutorialHUDWidget::NativeDestruct()
{
	// 購読したデリゲートは必ず解除する（成対 Hygiene）
	if (ATutorialGameMode* GameMode = Cast<ATutorialGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		GameMode->OnTutorialStepChanged.RemoveAll(this);
		GameMode->OnTutorialPlayerDone.RemoveAll(this);
		GameMode->OnTutorialComplete.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UTutorialHUDWidget::HandleStepChanged(ETutorialStep NewStep)
{
	ShowStep(NewStep);
}

void UTutorialHUDWidget::HandlePlayerDone(uint8 PlayerID, bool bDone)
{
	SetPlayerReady(PlayerID, bDone);
}

void UTutorialHUDWidget::HandleComplete()
{
	ShowComplete();
}
