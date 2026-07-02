// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimalGathererGameModeModeBase.h"
#include "GameFramework/GameUserSettings.h"	//bring the Framwork (FramSetting) from UE

void AAnimalGathererGameModeModeBase::BeginPlay()
{
	Super::BeginPlay();	//must begin the prantclass first (UE code include)

	FixGameResolution();
}

void AAnimalGathererGameModeModeBase::FixGameResolution()
{
	//get the game Settings from engine (by UE include
	if (UGameUserSettings* GameSettings = UGameUserSettings::GetGameUserSettings())
	{
		//these three from UE code itself
		GameSettings->SetScreenResolution(FIntPoint(1920, 1080));//set full HD 1920*1080
		GameSettings->SetFullscreenMode(EWindowMode::Windowed);//window mode
		GameSettings->ApplySettings(true);//use at once (save)
	}

}

