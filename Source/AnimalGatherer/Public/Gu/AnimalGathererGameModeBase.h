// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AnimalGathererGameModeBase.generated.h"

/**
 *
 */
UCLASS(Blueprintable)
class ANIMALGATHERER_API AAnimalGathererGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "GameSettings")
	void FixGameResolution();
};
