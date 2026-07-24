// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MyGameInstance.generated.h"

// 勝敗結果を表す列挙型
UENUM(BlueprintType)
enum class EMatchResult : uint8
{
    Draw = 0      UMETA(DisplayName = "引き分け"),
    P1Win = 1     UMETA(DisplayName = "1P 勝利"),
    P2Win = 2     UMETA(DisplayName = "2P 勝利")
};

UCLASS()
class ANIMALGATHERER_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
    // 最終結果を記録する関数
    void SetFinalResult(int32 InP1Score, int32 InP2Score);

    // UI等から参照するためのゲッター
    UFUNCTION(BlueprintCallable, Category = "GameResult")
    int32 GetP1Score() const { return P1Score; }

    UFUNCTION(BlueprintCallable, Category = "GameResult")
    int32 GetP2Score() const { return P2Score; }

    UFUNCTION(BlueprintCallable, Category = "GameResult")
    EMatchResult GetMatchResult() const { return MatchResult; }

protected:
    UPROPERTY(BlueprintReadOnly, Category = "GameResult")
    int32 P1Score = 0;

    UPROPERTY(BlueprintReadOnly, Category = "GameResult")
    int32 P2Score = 0;

    UPROPERTY(BlueprintReadOnly, Category = "GameResult")
    EMatchResult MatchResult = EMatchResult::Draw;
};
