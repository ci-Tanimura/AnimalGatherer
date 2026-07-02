// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AnimalGathererGameModeModeBase.generated.h"

/**
 *
 */
UCLASS()
class ANIMALGATHERER_API AAnimalGathererGameModeModeBase : public AGameModeBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "GameSettings")
	void FixGameResolution();
};
