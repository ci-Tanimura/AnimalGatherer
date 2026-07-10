// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Gu/AnimalGathererGameModeBase.h"
#include "Takeuchi/Actor/AnimalSpawner.h"
#include "MainGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScoreChangedSignature, int32, NewP1Score, int32, NewP2Score);

/**
 * 
 */
UCLASS()
class ANIMALGATHERER_API AMainGameMode : public AAnimalGathererGameModeBase
{
	GENERATED_BODY()
	
public:
	AMainGameMode();

    virtual void BeginPlay() override;

    // ゴールエリアから呼ばれるスコア加算
    UFUNCTION(BlueprintCallable, Category = "GameMode|Score")
    void AddScore(int32 PlayerID, int32 ScoreToAdd = 1);

    // タイムアップ時にゲームを終わらせる関数
    UFUNCTION(BlueprintCallable, Category = "GameMode|Flow")
    void EndGame();

    UPROPERTY(BlueprintAssignable, Category = "GameMode|Events")
    FOnScoreChangedSignature OnScoreChanged;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "GameMode|Score")
    int32 P1Score;

    UPROPERTY(BlueprintReadOnly, Category = "GameMode|Score")
    int32 P2Score;

private:
    // レベル上のスポーナーへの参照
    UPROPERTY()
    AAnimalSpawner* CachedAnimalSpawner;
};
